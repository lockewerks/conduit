#include "app/Application.h"
#include "app/KeychainStore.h"
#include "app/BrowserCredentials.h"
#include "util/Logger.h"
#include "util/Platform.h"
#include "util/TimeFormat.h"
#include "render/TextRenderer.h"
#include "render/EmojiMap.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <filesystem>
#include <fstream>
#include <algorithm>
#include "stb_image.h"
#include "stb_image_write.h"

#ifdef _WIN32
#include "stb_image_write.h"
#endif

namespace conduit {

// figure out where the executable lives so we can find assets relative to it
static std::string getExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#else
    return ".";
#endif
}

Application::Application() {}

Application::~Application() {
    shutdown();
}

bool Application::init() {
    // write logs to a file since we're a WIN32 app with no console
    Logger::instance().setFile("C:/Users/vexam/conduit_debug.log");
    LOG_INFO("conduit starting up...");

    if (!initSDL()) return false;
    if (!initOpenGL()) return false;
    if (!initImGui()) return false;

    loadFonts();
    ui_.theme().apply();

    pool_ = std::make_unique<ThreadPool>(4);

    loadConfig();
    setupKeybindings();
    registerCommands();
    setupTabCompletion();
    ui_.inputBar().setHistory(&input_history_);
    ui_.inputBar().setTabComplete(&tab_complete_);

    // autocomplete provider for @users, /commands, :emoji:, #channels
    ui_.inputBar().setAutocompleteProvider([this](char trigger, const std::string& prefix)
                                              -> std::vector<std::string> {
        std::vector<std::string> results;
        if (trigger == '@' && client_) {
            for (auto& u : client_->userCache().getAll()) {
                std::string name = u.effectiveName();
                if (prefix.empty() || name.find(prefix) == 0)
                    results.push_back(name);
                if (results.size() >= 20) break;
            }
        } else if (trigger == '/' ) {
            for (auto& cmd : commands_.commandNames()) {
                if (prefix.empty() || cmd.find(prefix) == 0)
                    results.push_back(cmd);
            }
        } else if (trigger == ':') {
            auto& emap = conduit::render::getEmojiMap();
            for (auto& [name, _] : emap) {
                if (prefix.empty() || name.find(prefix) == 0)
                    results.push_back(name);
                if (results.size() >= 20) break;
            }
        } else if (trigger == '#' && client_) {
            for (auto& ch : client_->getChannels()) {
                if (prefix.empty() || ch.name.find(prefix) == 0)
                    results.push_back(ch.name);
                if (results.size() >= 20) break;
            }
        }
        std::sort(results.begin(), results.end());
        return results;
    });

    connectToSlack();

    running_ = true;
    LOG_INFO("init complete, let's go");
    return true;
}

// ---- SDL / OpenGL / ImGui init (same as before, just tidied up) ----

bool Application::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        LOG_ERROR(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // tell windows we handle DPI ourselves
#ifdef _WIN32
    SetProcessDPIAware();

    // detect the OS display scale factor (125%, 150%, etc)
    // this becomes our default ui_scale_ so text isn't microscopic
    HDC hdc = GetDC(NULL);
    if (hdc) {
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ui_scale_ = (float)dpi / 96.0f; // 96 DPI = 100% scaling
        ReleaseDC(NULL, hdc);
        LOG_INFO("display DPI: " + std::to_string(dpi) + " -> scale: " +
                 std::to_string(ui_scale_));
    }
#endif

    // try to restore saved window position/size from last session
    int start_x = SDL_WINDOWPOS_CENTERED, start_y = SDL_WINDOWPOS_CENTERED;
    int start_w = 0, start_h = 0;
    bool maximized = false;

    std::string win_state_path = platform::getConfigDir() + "/window.state";
    {
        std::ifstream wf(win_state_path);
        if (wf.is_open()) {
            wf >> start_x >> start_y >> start_w >> start_h >> maximized;
            if (wf.fail() || start_w < 200 || start_h < 200) {
                start_x = SDL_WINDOWPOS_CENTERED;
                start_y = SDL_WINDOWPOS_CENTERED;
                start_w = 0;
                start_h = 0;
                maximized = false;
            }
            // pane widths are optional (added later) — don't let a read
            // failure here poison the window state we already read
            wf.clear();
            float bl_w = 0, nl_w = 0;
            wf >> bl_w >> nl_w;
            if (!wf.fail() && bl_w >= 100 && nl_w >= 100) {
                ui_.layout().buffer_list_width = bl_w;
                ui_.layout().nick_list_width = nl_w;
                panes_from_state_ = true;
            }
        }
    }

    // if no saved state, default to 80% of screen
    if (start_w == 0 || start_h == 0) {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
            start_w = dm.w * 4 / 5;
            start_h = dm.h * 4 / 5;
        } else {
            start_w = 1280;
            start_h = 900;
        }
    }

    window_ = SDL_CreateWindow(
        "Conduit",
        start_x, start_y,
        start_w, start_h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    if (window_ && maximized) {
        SDL_MaximizeWindow(window_);
    }

    if (!window_) {
        LOG_ERROR(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    // log what we actually got so we can see if SDL is lying to us
    int actual_w, actual_h, actual_x, actual_y;
    SDL_GetWindowSize(window_, &actual_w, &actual_h);
    SDL_GetWindowPosition(window_, &actual_x, &actual_y);
    Uint32 actual_flags = SDL_GetWindowFlags(window_);
    LOG_INFO("window created: " + std::to_string(actual_w) + "x" + std::to_string(actual_h) +
             " at " + std::to_string(actual_x) + "," + std::to_string(actual_y) +
             " flags=0x" + std::to_string(actual_flags));
    if (actual_flags & SDL_WINDOW_FULLSCREEN) LOG_WARN("FULLSCREEN flag is set!");
    if (actual_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) LOG_WARN("FULLSCREEN_DESKTOP flag is set!");
    if (actual_flags & SDL_WINDOW_BORDERLESS) LOG_WARN("BORDERLESS flag is set!");
    if (actual_flags & SDL_WINDOW_MAXIMIZED) LOG_WARN("MAXIMIZED flag is set!");

    // set window icon - try a few paths because MSVC puts the exe in weird places
    {
        std::string exe_dir = getExeDir();
        std::vector<std::string> icon_paths = {
            exe_dir + "/assets/icons/conduit.png",
            exe_dir + "/../assets/icons/conduit.png",
            "assets/icons/conduit.png",
        };
        for (auto& path : icon_paths) {
            int iw, ih, ic;
            unsigned char* pixels = stbi_load(path.c_str(), &iw, &ih, &ic, 4);
            if (pixels) {
                SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(pixels, iw, ih, 32, iw * 4,
                    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
                if (icon) {
                    SDL_SetWindowIcon(window_, icon);
                    SDL_FreeSurface(icon);
                }
                stbi_image_free(pixels);
                break;
            }
        }
    }

    return true;
}

bool Application::initOpenGL() {
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        LOG_ERROR(std::string("GL context creation failed: ") + SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);
    return true;
}

bool Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 330");
    return true;
}

void Application::loadFonts() {
    // use ui_scale_ for initial font size so text matches the OS DPI setting
    float font_size = 14.0f * ui_scale_;
    rebuildFonts();
    LOG_INFO("initial font size: " + std::to_string(font_size) + "px (scale " +
             std::to_string(ui_scale_) + ")");
}

// ---- Config & Slack connection ----

void Application::loadConfig() {
    std::string config_path = Config::defaultPath();
    config_.load(config_path);
    auto& cfg = config_.get();

    // apply UI config — pane widths from window.state take priority
    // (panes_from_state_ is set in initSDL when window.state has widths)
    if (!panes_from_state_) {
        ui_.layout().buffer_list_width = cfg.ui.buffer_list_width;
        ui_.layout().nick_list_width = cfg.ui.nick_list_width;
    }
    ui_.layout().show_buffer_list = cfg.ui.show_buffer_list;
    ui_.layout().show_nick_list = cfg.ui.show_nick_list;

    LOG_INFO("config loaded");
}

void Application::connectToSlack() {
    auto& orgs = config_.get().orgs;

    // check keychain for a previously saved token
    std::string user_token;
    std::string app_token;
    std::string org_name = "Slack";

    if (!orgs.empty()) {
        auto& org = orgs[0];
        org_name = org.name;
        user_token = org.user_token;
        app_token = org.app_token;
    }

    // try the keychain if no token in config
    if (user_token.empty()) {
        auto stored = KeychainStore::retrieve("conduit", "user_token");
        if (stored) {
            user_token = *stored;
            LOG_INFO("loaded token from keychain");
        }
    }
    // xoxc tokens need a d cookie
    std::string d_cookie;
    if (!orgs.empty()) d_cookie = orgs[0].d_cookie;
    if (d_cookie.empty()) {
        auto stored = KeychainStore::retrieve("conduit", "d_cookie");
        if (stored) d_cookie = *stored;
    }

    // no token? try to steal it from a Chromium browser before asking the user
    if (user_token.empty()) {
        LOG_INFO("no token in config or keychain, scanning browsers...");
        ui_.statusBar().setConnectionState("scanning browsers...");

        auto creds = BrowserCredentials::scan();
        if (!creds.empty()) {
            auto& best = creds[0];
            user_token = best.token;
            d_cookie = best.cookie;
            LOG_INFO("swiped credentials from " + best.browser_name);

            // save to keychain so we don't have to do this again
            KeychainStore::store("conduit", "user_token", user_token);
            if (!d_cookie.empty()) {
                KeychainStore::store("conduit", "d_cookie", d_cookie);
            }
        }
    }

    // still no token? fall back to manual paste
    if (user_token.empty()) {
        LOG_INFO("no token found anywhere, prompting user to paste one");
        ui_.statusBar().setConnectionState("paste your token");
        ui_.statusBar().setOrgName("Conduit");
        ui_.inputBar().setChannelName("paste xoxc- token");
        auth_state_ = AuthState::WaitingForToken;
        return;
    }

    // we have a token (from config, keychain, or a previous OAuth).
    // set up database and connect.
    std::string data_dir = platform::getDataDir();
    platform::ensureDir(data_dir);
    db_ = std::make_unique<cache::Database>();
    db_->open(data_dir + "/" + org_name + ".db");

    msg_cache_ = std::make_unique<cache::MessageCache>(*db_);

    OrgConfig resolved;
    resolved.name = org_name;
    resolved.user_token = user_token;
    resolved.app_token = app_token;
    resolved.d_cookie = d_cookie;

    client_ = std::make_unique<slack::SlackClient>(resolved, *db_);

    resolved_token_ = user_token;
    ui_.bufferView().setImageRenderer(&image_renderer_);
    ui_.bufferView().setGifRenderer(&gif_renderer_);
    ui_.bufferView().setAuthToken(user_token);
    ui_.bufferView().setSelfUserId(client_->selfUserId());

    // thread panel needs display name resolution and reply handling
    ui_.threadPanel().setDisplayNameFn([this](const std::string& uid) -> std::string {
        return client_ ? client_->displayName(uid) : uid;
    });
    ui_.threadPanel().setReplyCallback([this](const slack::ChannelId& ch,
                                              const slack::Timestamp& ts,
                                              const std::string& text) {
        pool_->enqueue([this, ch, ts, text]() {
            client_->sendMessage(ch, text, ts);
        });
    });

    // xoxc tokens need the d cookie on image/gif downloads too
    if (!d_cookie.empty()) {
        image_renderer_.setCookie(d_cookie);
        gif_renderer_.setCookie(d_cookie);
        emoji_renderer_.setCookie(d_cookie);
    }
    emoji_renderer_.setAuthToken(user_token);
    emoji_renderer_.setThreadPool(pool_.get());

    // pass emoji renderer to UI components
    ui_.bufferView().setEmojiRenderer(&emoji_renderer_);

    ui_.statusBar().setOrgName(org_name);
    ui_.statusBar().setConnectionState("connecting...");

    pool_->enqueue([this, name = org_name]() {
        if (client_->connect()) {
            needs_channel_sync_ = true;
            LOG_INFO("connected to " + name);
            // fetch custom workspace emoji in background
            auto custom = client_->getCustomEmoji();
            if (!custom.empty()) {
                emoji_renderer_.setCustomEmoji(custom);
            }
        } else {
            LOG_ERROR("failed to connect to " + name);
        }
    });
}

// ---- Slash commands ----

void Application::registerCommands() {
    commands_.registerCommand("join", "Join a channel", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty()) return;
        std::string ch = cmd.argv[0];
        if (ch[0] == '#') ch = ch.substr(1);
        // find channel by name
        if (!client_) return;
        for (auto& c : client_->getChannels()) {
            if (c.name == ch) {
                client_->joinChannel(c.id);
                needs_channel_sync_ = true;
                return;
            }
        }
        LOG_WARN("channel not found: " + ch);
    });

    commands_.registerCommand("leave", "Leave current channel", [this](const input::ParsedCommand&) {
        if (!client_ || active_channel_.empty()) return;
        client_->leaveChannel(active_channel_);
        needs_channel_sync_ = true;
    });

    commands_.registerCommand("part", "Leave current channel", [this](const input::ParsedCommand& cmd) {
        commands_.execute("/leave");
    });

    commands_.registerCommand("msg", "Send a DM: /msg @user message", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty() || !client_) return;
        std::string target = cmd.argv[0];
        if (target[0] == '@') target = target.substr(1);

        // find the user
        std::string user_id;
        for (auto& u : client_->userCache().getAll()) {
            if (u.effectiveName() == target || u.display_name == target || u.real_name == target) {
                user_id = u.id;
                break;
            }
        }
        if (user_id.empty()) {
            LOG_WARN("user not found: " + target);
            return;
        }

        // extract message text (everything after the username)
        std::string text;
        auto space_pos = cmd.args.find(' ');
        if (space_pos != std::string::npos) text = cmd.args.substr(space_pos + 1);

        pool_->enqueue([this, user_id, text]() {
            auto ch_id = client_->openDM(user_id);
            if (!ch_id) {
                LOG_ERROR("failed to open DM with " + user_id);
                return;
            }
            if (!text.empty()) {
                client_->sendMessage(*ch_id, text);
            }
            {
                std::lock_guard<std::mutex> lock(pending_switch_mutex_);
                pending_switch_channel_ = *ch_id;
            }
            needs_channel_sync_ = true;
        });
    });

    commands_.registerCommand("topic", "Set channel topic", [this](const input::ParsedCommand& cmd) {
        if (cmd.args.empty() || !client_ || active_channel_.empty()) return;
        client_->setTopic(active_channel_, cmd.args);
        ui_.titleBar().setTopic(cmd.args);
    });

    commands_.registerCommand("me", "Send action message", [this](const input::ParsedCommand& cmd) {
        if (cmd.args.empty() || !client_ || active_channel_.empty()) return;
        client_->sendMessage(active_channel_, "_" + cmd.args + "_");
    });

    commands_.registerCommand("react", "React to last message: /react :emoji:", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty() || !client_ || active_channel_.empty()) return;
        std::string emoji = cmd.argv[0];
        // strip colons
        if (emoji.front() == ':') emoji = emoji.substr(1);
        if (!emoji.empty() && emoji.back() == ':') emoji.pop_back();
        // get last message ts
        auto msgs = msg_cache_->get(active_channel_, 1);
        if (!msgs.empty()) {
            client_->addReaction(active_channel_, msgs.back().ts, emoji);
        }
    });

    commands_.registerCommand("unreact", "Remove reaction: /unreact :emoji:", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty() || !client_ || active_channel_.empty()) return;
        std::string emoji = cmd.argv[0];
        if (emoji.front() == ':') emoji = emoji.substr(1);
        if (!emoji.empty() && emoji.back() == ':') emoji.pop_back();
        auto msgs = msg_cache_->get(active_channel_, 1);
        if (!msgs.empty()) {
            client_->removeReaction(active_channel_, msgs.back().ts, emoji);
        }
    });

    commands_.registerCommand("search", "Search messages: /search query", [this](const input::ParsedCommand& cmd) {
        if (cmd.args.empty() || !client_) return;
        ui_.searchPanel().open();
        pool_->enqueue([this, query = cmd.args]() {
            auto results = client_->searchMessages(query);
            std::vector<slack::Message> msgs;
            for (auto& r : results) msgs.push_back(r.message);
            ui_.searchPanel().setResults(msgs);
        });
    });

    commands_.registerCommand("upload", "Upload a file: /upload path", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty() || !client_ || active_channel_.empty()) return;
        std::string path = cmd.args;
        pool_->enqueue([this, path]() {
            client_->uploadFile(active_channel_, path);
        });
    });

    commands_.registerCommand("status", "Set status: /status :emoji: text", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty() || !client_) return;
        std::string emoji = cmd.argv[0];
        std::string text = cmd.argv.size() > 1 ? cmd.args.substr(cmd.args.find(' ') + 1) : "";
        client_->setStatus(emoji, text);
    });

    commands_.registerCommand("away", "Toggle away status", [this](const input::ParsedCommand&) {
        if (!client_) return;
        client_->setPresence(true);
    });

    commands_.registerCommand("clear", "Clear buffer display", [this](const input::ParsedCommand&) {
        // just force a re-render
        needs_message_sync_ = true;
    });

    commands_.registerCommand("close", "Close current buffer", [this](const input::ParsedCommand&) {
        // switch to the next buffer
        ui_.bufferList().selectNext();
        int idx = ui_.bufferList().selectedIndex();
        switchToBuffer(idx);
    });

    commands_.registerCommand("reconnect", "Force reconnect", [this](const input::ParsedCommand&) {
        if (!client_) return;
        client_->disconnect();
        ui_.statusBar().setConnectionState("reconnecting...");
        pool_->enqueue([this]() { client_->connect(); });
    });

    commands_.registerCommand("reauth", "Clear cached credentials and re-scan browser", [this](const input::ParsedCommand&) {
        LOG_INFO("clearing cached credentials...");
        // nuke the keychain entries so connectToSlack() will rescan the browser
        KeychainStore::remove("conduit", "user_token");
        KeychainStore::remove("conduit", "d_cookie");

        // disconnect the current session
        if (client_) {
            client_->disconnect();
            client_.reset();
        }
        msg_cache_.reset();
        db_.reset();

        ui_.statusBar().setConnectionState("re-authenticating...");
        LOG_INFO("credentials cleared, rescanning browsers...");

        // reconnect with fresh credentials
        connectToSlack();
    });

    commands_.registerCommand("pin", "Pin selected message", [this](const input::ParsedCommand&) {
        if (!client_ || active_channel_.empty()) return;
        auto msgs = msg_cache_->get(active_channel_, 1);
        if (!msgs.empty()) {
            client_->pinMessage(active_channel_, msgs.back().ts);
        }
    });

    commands_.registerCommand("unpin", "Unpin selected message", [this](const input::ParsedCommand&) {
        if (!client_ || active_channel_.empty()) return;
        auto msgs = msg_cache_->get(active_channel_, 1);
        if (!msgs.empty()) {
            client_->unpinMessage(active_channel_, msgs.back().ts);
        }
    });

    commands_.registerCommand("thread", "Open thread panel", [this](const input::ParsedCommand&) {
        LOG_INFO("thread panel - select a message first");
    });

    commands_.registerCommand("query", "Open DM with user: /query @user", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty() || !client_) return;
        std::string target = cmd.argv[0];
        if (target[0] == '@') target = target.substr(1);

        // find the user by name
        std::string user_id;
        for (auto& u : client_->userCache().getAll()) {
            if (u.effectiveName() == target || u.display_name == target || u.real_name == target) {
                user_id = u.id;
                break;
            }
        }
        if (user_id.empty()) {
            LOG_WARN("user not found: " + target);
            return;
        }

        // check if we already have a DM channel open
        for (auto& ch : client_->channelCache().getJoined()) {
            if (ch.type == slack::ChannelType::DirectMessage && ch.dm_user_id == user_id) {
                auto& entries = ui_.bufferList().entries();
                for (int i = 0; i < (int)entries.size(); i++) {
                    if (entries[i].channel_id == ch.id) {
                        ui_.bufferList().select(i);
                        switchToBuffer(i);
                        return;
                    }
                }
            }
        }

        // no existing DM, open one via the API
        pool_->enqueue([this, user_id]() {
            auto ch_id = client_->openDM(user_id);
            if (!ch_id) {
                LOG_ERROR("failed to open DM");
                return;
            }
            {
                std::lock_guard<std::mutex> lock(pending_switch_mutex_);
                pending_switch_channel_ = *ch_id;
            }
            needs_channel_sync_ = true;
        });
    });

    commands_.registerCommand("reply", "Reply in thread: /reply text", [this](const input::ParsedCommand& cmd) {
        if (cmd.args.empty() || !client_ || active_channel_.empty()) return;
        // reply to the last message that started a thread, or the last message period
        auto msgs = msg_cache_->get(active_channel_, 50);
        if (msgs.empty()) return;
        // find the last thread parent or just use the last message
        std::string thread_ts;
        for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
            if (it->reply_count > 0) { thread_ts = it->ts; break; }
        }
        if (thread_ts.empty()) thread_ts = msgs.back().ts;
        pool_->enqueue([this, ch = active_channel_, ts = thread_ts, text = cmd.args]() {
            client_->sendMessage(ch, text, ts);
        });
    });

    commands_.registerCommand("edit", "Edit last message: /edit new text", [this](const input::ParsedCommand& cmd) {
        if (cmd.args.empty() || !client_ || active_channel_.empty()) return;
        // find the last message sent by us
        auto msgs = msg_cache_->get(active_channel_, 50);
        for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
            if (it->user == client_->selfUserId()) {
                pool_->enqueue([this, ch = active_channel_, ts = it->ts, text = cmd.args]() {
                    client_->editMessage(ch, ts, text);
                });
                return;
            }
        }
        LOG_WARN("no own message found to edit");
    });

    commands_.registerCommand("delete", "Delete last own message", [this](const input::ParsedCommand&) {
        if (!client_ || active_channel_.empty()) return;
        auto msgs = msg_cache_->get(active_channel_, 50);
        for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
            if (it->user == client_->selfUserId()) {
                pool_->enqueue([this, ch = active_channel_, ts = it->ts]() {
                    client_->deleteMessage(ch, ts);
                });
                return;
            }
        }
        LOG_WARN("no own message found to delete");
    });

    commands_.registerCommand("set", "Change setting: /set key=value", [this](const input::ParsedCommand& cmd) {
        if (cmd.args.empty()) {
            LOG_INFO("usage: /set key=value (e.g. /set font_size=16)");
            return;
        }
        auto eq = cmd.args.find('=');
        if (eq == std::string::npos) {
            LOG_WARN("invalid format, use /set key=value");
            return;
        }
        std::string key = cmd.args.substr(0, eq);
        std::string val = cmd.args.substr(eq + 1);
        if (key == "font_size") {
            try { config_.get().font_size = std::stof(val); } catch (...) {}
        } else if (key == "buffer_list_width") {
            try { ui_.layout().buffer_list_width = std::stof(val); } catch (...) {}
        } else if (key == "nick_list_width") {
            try { ui_.layout().nick_list_width = std::stof(val); } catch (...) {}
        } else {
            LOG_WARN("unknown setting: " + key);
        }
    });

    commands_.registerCommand("org", "Org management: /org list|switch|connect|disconnect", [this](const input::ParsedCommand& cmd) {
        if (cmd.argv.empty()) {
            LOG_INFO("usage: /org list | /org switch <name> | /org connect | /org disconnect <name>");
            return;
        }
        std::string sub = cmd.argv[0];
        if (sub == "list") {
            for (auto& org : config_.get().orgs) {
                std::string status = (client_ && client_->teamName() == org.name) ? "connected" : "not connected";
                LOG_INFO("  " + org.name + " [" + status + "]");
            }
        } else if (sub == "switch" && cmd.argv.size() > 1) {
            LOG_INFO("multi-org switching coming soon (only single org supported for now)");
        } else if (sub == "connect") {
            LOG_INFO("add org to config file and restart for now");
        } else if (sub == "disconnect") {
            if (client_) { client_->disconnect(); ui_.statusBar().setConnectionState("disconnected"); }
        }
    });

    commands_.registerCommand("help", "Show available commands", [this](const input::ParsedCommand&) {
        LOG_INFO("\n" + commands_.allHelp());
    });

    commands_.registerCommand("debug", "Toggle debug log buffer", [this](const input::ParsedCommand&) {
        auto recent = Logger::instance().getRecent(50);
        for (auto& line : recent) {
            LOG_INFO(line);
        }
    });
}

// ---- Keybindings ----

void Application::setupKeybindings() {
    keys_.bind("toggle_buffer_list", SDLK_F5, 0);
    keys_.bind("toggle_buffer_list", SDLK_F6, 0);
    keys_.bind("toggle_nick_list", SDLK_F7, 0);
    keys_.bind("toggle_nick_list", SDLK_F8, 0);
    keys_.bind("prev_buffer", SDLK_LEFT, KMOD_ALT);
    keys_.bind("next_buffer", SDLK_RIGHT, KMOD_ALT);
    keys_.bind("next_unread", SDLK_a, KMOD_ALT);
    keys_.bind("open_search", SDLK_f, KMOD_CTRL);
    keys_.bind("open_palette", SDLK_k, KMOD_CTRL);
    keys_.bind("org_switcher", SDLK_n, KMOD_ALT);
    keys_.bind("open_thread", SDLK_t, KMOD_ALT);
    keys_.bind("escape", SDLK_ESCAPE, 0);
    keys_.bind("page_up", SDLK_PAGEUP, 0);
    keys_.bind("page_down", SDLK_PAGEDOWN, 0);

    keys_.onAction("toggle_buffer_list", [this]() { ui_.toggleBufferList(); });
    keys_.onAction("toggle_nick_list", [this]() { ui_.toggleNickList(); });
    keys_.onAction("prev_buffer", [this]() {
        ui_.bufferList().selectPrev();
        switchToBuffer(ui_.bufferList().selectedIndex());
    });
    keys_.onAction("next_buffer", [this]() {
        ui_.bufferList().selectNext();
        switchToBuffer(ui_.bufferList().selectedIndex());
    });
    keys_.onAction("next_unread", [this]() {
        // find the next buffer with unreads
        auto& entries = ui_.bufferList().entries();
        int start = ui_.bufferList().selectedIndex();
        for (int i = 1; i < (int)entries.size(); i++) {
            int idx = (start + i) % (int)entries.size();
            if (entries[idx].has_unread && !entries[idx].is_separator) {
                ui_.bufferList().select(idx);
                switchToBuffer(idx);
                return;
            }
        }
    });
    keys_.onAction("open_search", [this]() {
        if (ui_.searchPanel().isOpen()) {
            ui_.searchPanel().close();
        } else {
            ui_.searchPanel().open();
        }
    });
    keys_.onAction("open_palette", [this]() {
        if (ui_.commandPalette().isOpen()) {
            ui_.commandPalette().close();
        } else {
            // populate palette with channels and commands
            std::vector<ui::CommandPalette::Entry> entries;
            if (client_) {
                for (auto& ch : client_->getChannels()) {
                    if (!ch.is_member) continue;
                    bool is_dm = (ch.type == slack::ChannelType::DirectMessage);
                    entries.push_back({
                        is_dm ? "@" + ch.name : "#" + ch.name,
                        ch.id,
                        is_dm ? "dm" : "channel"
                    });
                }
            }
            for (auto& cmd : commands_.commandNames()) {
                entries.push_back({"/" + cmd, cmd, "command"});
            }
            ui_.commandPalette().setEntries(entries);
            ui_.commandPalette().setSelectCallback([this](const ui::CommandPalette::Entry& e) {
                if (e.category == "command") {
                    commands_.execute("/" + e.value);
                } else {
                    // switch to this channel
                    auto& entries = ui_.bufferList().entries();
                    for (int i = 0; i < (int)entries.size(); i++) {
                        if (entries[i].channel_id == e.value) {
                            ui_.bufferList().select(i);
                            switchToBuffer(i);
                            break;
                        }
                    }
                }
            });
            ui_.commandPalette().open();
        }
    });
    keys_.onAction("open_thread", [this]() {
        if (!client_ || active_channel_.empty()) return;
        // use selected message if available, otherwise last message
        std::string thread_ts = ui_.bufferView().selectedTs();
        if (thread_ts.empty()) {
            auto msgs = msg_cache_->get(active_channel_, 1);
            if (!msgs.empty()) thread_ts = msgs.back().ts;
        }
        if (thread_ts.empty()) return;
        ui_.threadPanel().open(active_channel_, thread_ts);
        pool_->enqueue([this, ch = active_channel_, ts = thread_ts]() {
            auto replies = client_->getThreadReplies(ch, ts);
            ui_.threadPanel().setMessages(replies);
        });
    });
    keys_.onAction("escape", [this]() {
        if (ui_.emojiPicker().isOpen()) ui_.emojiPicker().close();
        else if (ui_.searchPanel().isOpen()) ui_.searchPanel().close();
        else if (ui_.commandPalette().isOpen()) ui_.commandPalette().close();
        else if (ui_.threadPanel().isOpen()) ui_.threadPanel().close();
    });

    // alt+1-9 buffer switching
    for (int i = 1; i <= 9; i++) {
        keys_.bind("buf" + std::to_string(i), SDLK_0 + i, KMOD_ALT);
        keys_.onAction("buf" + std::to_string(i), [this, i]() {
            // skip separator entries
            int target = i;
            auto& entries = ui_.bufferList().entries();
            int buf_count = 0;
            for (int j = 0; j < (int)entries.size(); j++) {
                if (!entries[j].is_separator) {
                    buf_count++;
                    if (buf_count == target) {
                        ui_.bufferList().select(j);
                        switchToBuffer(j);
                        return;
                    }
                }
            }
        });
    }

    // alt+up/down: prev/next buffer with unread
    keys_.bind("prev_unread", SDLK_UP, KMOD_ALT);
    keys_.bind("next_unread_down", SDLK_DOWN, KMOD_ALT);
    keys_.onAction("prev_unread", [this]() {
        auto& entries = ui_.bufferList().entries();
        int start = ui_.bufferList().selectedIndex();
        for (int i = (int)entries.size() - 1; i >= 1; i--) {
            int idx = (start - i + (int)entries.size()) % (int)entries.size();
            if (entries[idx].has_unread && !entries[idx].is_separator) {
                ui_.bufferList().select(idx);
                switchToBuffer(idx);
                return;
            }
        }
    });
    keys_.onAction("next_unread_down", [this]() {
        auto& entries = ui_.bufferList().entries();
        int start = ui_.bufferList().selectedIndex();
        for (int i = 1; i < (int)entries.size(); i++) {
            int idx = (start + i) % (int)entries.size();
            if (entries[idx].has_unread && !entries[idx].is_separator) {
                ui_.bufferList().select(idx);
                switchToBuffer(idx);
                return;
            }
        }
    });

    // ctrl+l: clear buffer display
    keys_.bind("clear_buffer", SDLK_l, KMOD_CTRL);
    keys_.onAction("clear_buffer", [this]() { needs_message_sync_ = true; });

    // alt+e: edit last own message
    keys_.bind("edit_msg", SDLK_e, KMOD_ALT);
    keys_.onAction("edit_msg", [this]() { commands_.execute("/edit "); });

    // alt+d: delete last own message
    keys_.bind("delete_msg", SDLK_d, KMOD_ALT);
    keys_.onAction("delete_msg", [this]() { commands_.execute("/delete"); });

    // alt+r: open reaction picker for selected (or last) message
    keys_.bind("react_picker", SDLK_r, KMOD_ALT);
    keys_.onAction("react_picker", [this]() {
        if (!client_ || active_channel_.empty() || !msg_cache_) return;

        // figure out which message we're reacting to
        std::string ts = ui_.bufferView().selectedTs();
        if (ts.empty()) {
            auto msgs = msg_cache_->get(active_channel_, 1);
            if (!msgs.empty()) ts = msgs.back().ts;
        }
        if (ts.empty()) return;

        // anchor near the input bar so it doesn't cover the message
        ImVec2 vp_size = ImGui::GetMainViewport()->Size;
        float anchor_x = vp_size.x * 0.3f;
        float anchor_y = vp_size.y - ui_.layout().input_bar_height - ui_.layout().status_bar_height;

        ui_.emojiPicker().setSelectCallback([this, ch = active_channel_, ts](const std::string& emoji) {
            pool_->enqueue([this, ch, ts, emoji]() {
                client_->addReaction(ch, ts, emoji);
            });
        });
        ui_.emojiPicker().open(anchor_x, anchor_y);
    });

    // home/end: scroll to top/bottom
    keys_.bind("scroll_top", SDLK_HOME, 0);
    keys_.bind("scroll_bottom", SDLK_END, 0);
    keys_.onAction("scroll_top", [this]() {
        // trigger loading older history
        if (client_ && !active_channel_.empty()) {
            auto msgs = msg_cache_->get(active_channel_, 1);
            if (!msgs.empty()) {
                pool_->enqueue([this, ch = active_channel_, oldest = msgs.front().ts]() {
                    auto older = client_->getHistory(ch, 100, oldest);
                    if (!older.empty()) {
                        msg_cache_->store(ch, older);
                        needs_message_sync_ = true;
                    }
                });
            }
        }
    });
    keys_.onAction("scroll_bottom", [this]() { ui_.bufferView().scrollToBottom(); });
}

// ---- Tab completion ----

void Application::setupTabCompletion() {
    // @user completion
    tab_complete_.addProvider('@', [this](const std::string& prefix) -> std::vector<std::string> {
        std::vector<std::string> names;
        if (!client_) return names;
        for (auto& u : client_->userCache().getAll()) {
            std::string name = u.effectiveName();
            if (prefix.empty() || name.find(prefix) == 0) {
                names.push_back(name);
            }
        }
        return names;
    });

    // #channel completion
    tab_complete_.addProvider('#', [this](const std::string& prefix) -> std::vector<std::string> {
        std::vector<std::string> names;
        if (!client_) return names;
        for (auto& ch : client_->getChannels()) {
            if (prefix.empty() || ch.name.find(prefix) == 0) {
                names.push_back(ch.name);
            }
        }
        return names;
    });

    // :emoji: completion (basic - just common ones until we load the full list)
    tab_complete_.addProvider(':', [this](const std::string& prefix) -> std::vector<std::string> {
        static const std::vector<std::string> common = {
            "thumbsup", "thumbsdown", "heart", "fire", "eyes", "rocket",
            "tada", "white_check_mark", "x", "100", "wave", "pray",
            "laughing", "cry", "thinking_face", "raised_hands", "clap",
            "+1", "-1", "ok_hand", "muscle", "skull", "shrug",
        };
        std::vector<std::string> result;
        for (auto& e : common) {
            if (prefix.empty() || e.find(prefix) == 0) result.push_back(e);
        }
        return result;
    });

    // /command completion
    tab_complete_.addProvider('/', [this](const std::string& prefix) -> std::vector<std::string> {
        std::vector<std::string> result;
        for (auto& cmd : commands_.commandNames()) {
            if (prefix.empty() || cmd.find(prefix) == 0) result.push_back(cmd);
        }
        return result;
    });
}

// ---- Main loop ----

void Application::run() {
    int last_buf_idx = -1;

    while (running_) {
        processInput();
        processSlackEvents();

        // sync UI with slack data when needed
        if (needs_channel_sync_ && client_ && client_->isConnected()) {
            syncBufferList();
            needs_channel_sync_ = false;

            // if a DM was opened in the background, switch to it now
            {
                std::lock_guard<std::mutex> lock(pending_switch_mutex_);
                if (!pending_switch_channel_.empty()) {
                    bool found = false;
                    auto& entries = ui_.bufferList().entries();
                    for (int i = 0; i < (int)entries.size(); i++) {
                        if (entries[i].channel_id == pending_switch_channel_) {
                            ui_.bufferList().select(i);
                            switchToBuffer(i);
                            last_buf_idx = i;
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        pending_switch_channel_.clear();
                        pending_switch_retries_ = 0;
                    } else if (++pending_switch_retries_ > 5) {
                        LOG_WARN("gave up waiting for DM channel " + pending_switch_channel_);
                        pending_switch_channel_.clear();
                        pending_switch_retries_ = 0;
                    } else {
                        // channel not in buffer list yet, force another sync next frame
                        needs_channel_sync_ = true;
                    }
                }
            } // lock_guard
        }
        if (needs_message_sync_ && client_ && client_->isConnected() && !active_channel_.empty()) {
            syncBufferView();
            syncNickList();
            needs_message_sync_ = false;
        }

        // update status bar
        if (client_) {
            ui_.statusBar().setConnectionState(client_->connectionState());
        }

        // no more polling! websocket delivers events in real-time now.

        // push any decoded images/gifs to the GPU (GL calls must happen on main thread)
        image_renderer_.uploadPending();
        emoji_renderer_.uploadPending();
        gif_renderer_.uploadPending();

        // tick GIF animations so they don't just sit there frozen
        float now = (float)SDL_GetTicks() / 1000.0f;
        float delta = now - last_frame_time_;
        last_frame_time_ = now;
        gif_renderer_.update(delta * 1000.0f);

        // prune expired typing indicators (they time out after 5 seconds)
        {
            auto now_tp = std::chrono::steady_clock::now();
            typing_events_.erase(
                std::remove_if(typing_events_.begin(), typing_events_.end(),
                    [&](auto& p) { return now_tp >= p.second; }),
                typing_events_.end());
            std::vector<std::string> typers;
            for (auto& te : typing_events_) typers.push_back(te.first);
            ui_.statusBar().setTypingUsers(typers);
        }

        // rebuild fonts if scale changed (ctrl+/-)
        if (fonts_need_rebuild_) {
            rebuildFonts();
            ui_.theme().apply(); // re-apply theme after style reset
            fonts_need_rebuild_ = false;
        }

        // ---- imgui frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ui_.render();

        // right-click context menu actions
        if (ui_.wantsPasteImage()) {
            tryPasteClipboardImage();
            ui_.clearPasteImage();
        }
        if (ui_.wantsPasteText()) {
            const char* clip = SDL_GetClipboardText();
            if (clip && clip[0]) {
                ui_.inputBar().pasteText(clip);
            }
            ui_.clearPasteText();
        }

        // detect channel switch from mouse clicks (imgui processes clicks during render)
        int cur_buf = ui_.bufferList().selectedIndex();
        if (cur_buf != last_buf_idx && cur_buf >= 0) {
            last_buf_idx = cur_buf;
            switchToBuffer(cur_buf);
        }

        // right-click context menu actions on messages
        {
            auto ctx = ui_.bufferView().lastContextAction();
            if (ctx.type != ui::BufferView::ContextAction::None && client_) {
                switch (ctx.type) {
                case ui::BufferView::ContextAction::Copy:
                    SDL_SetClipboardText(ctx.text.c_str());
                    break;

                case ui::BufferView::ContextAction::Reply: {
                    std::string ts = ctx.ts;
                    ui_.threadPanel().open(active_channel_, ts);
                    pool_->enqueue([this, ch = active_channel_, ts]() {
                        auto replies = client_->getThreadReplies(ch, ts);
                        ui_.threadPanel().setMessages(replies);
                    });
                    break;
                }

                case ui::BufferView::ContextAction::React: {
                    // open the full emoji picker anchored near the input bar
                    std::string ts = ctx.ts;
                    ImVec2 vp_size = ImGui::GetMainViewport()->Size;
                    float anchor_x = vp_size.x * 0.3f;
                    float anchor_y = vp_size.y - ui_.layout().input_bar_height - ui_.layout().status_bar_height;
                    ui_.emojiPicker().setSelectCallback([this, ch = active_channel_, ts](const std::string& emoji) {
                        pool_->enqueue([this, ch, ts, emoji]() {
                            client_->addReaction(ch, ts, emoji);
                        });
                    });
                    ui_.emojiPicker().open(anchor_x, anchor_y);
                    break;
                }

                case ui::BufferView::ContextAction::QuickReact: {
                    std::string ts = ctx.ts;
                    std::string emoji = ctx.emoji;
                    pool_->enqueue([this, ch = active_channel_, ts, emoji]() {
                        client_->addReaction(ch, ts, emoji);
                    });
                    break;
                }

                case ui::BufferView::ContextAction::Edit: {
                    editing_ts_ = ctx.ts;
                    ui_.inputBar().setText(ctx.text);
                    break;
                }

                case ui::BufferView::ContextAction::Delete: {
                    std::string ts = ctx.ts;
                    pool_->enqueue([this, ch = active_channel_, ts]() {
                        client_->deleteMessage(ch, ts);
                    });
                    break;
                }

                default: break;
                }
            }
            ui_.bufferView().clearContextAction();
        }

        // nick list click: open DM with that user
        {
            std::string nick = ui_.nickList().lastClickedNick();
            if (!nick.empty() && client_) {
                // resolve nick to user ID
                std::string user_id;
                for (auto& u : client_->userCache().getAll()) {
                    if (u.effectiveName() == nick || u.display_name == nick) {
                        user_id = u.id;
                        break;
                    }
                }
                if (!user_id.empty()) {
                    pool_->enqueue([this, user_id]() {
                        auto ch_id = client_->openDM(user_id);
                        if (!ch_id) return;
                        {
                            std::lock_guard<std::mutex> lock(pending_switch_mutex_);
                            pending_switch_channel_ = *ch_id;
                        }
                        needs_channel_sync_ = true;
                    });
                }
            }
            ui_.nickList().clearClickedNick();
        }

        // reaction badge clicks: toggle add/remove on the Slack API
        {
            auto rc = ui_.bufferView().lastReactionClick();
            if (rc.clicked && client_ && !active_channel_.empty()) {
                std::string emoji = rc.emoji_name;
                std::string ts = rc.message_ts;

                // check if we already reacted so we know whether to add or yank it
                bool already_reacted = false;
                auto msgs = msg_cache_->get(active_channel_, 200);
                for (auto& m : msgs) {
                    if (m.ts == ts) {
                        for (auto& r : m.reactions) {
                            if (r.emoji_name == emoji && r.user_reacted) {
                                already_reacted = true;
                                break;
                            }
                        }
                        break;
                    }
                }

                pool_->enqueue([this, ch = active_channel_, ts, emoji, already_reacted]() {
                    if (already_reacted) {
                        client_->removeReaction(ch, ts, emoji);
                    } else {
                        client_->addReaction(ch, ts, emoji);
                    }
                    // refresh so the UI reflects the change without waiting for the socket
                    auto history = client_->getHistory(ch, 50);
                    msg_cache_->store(ch, history);
                    needs_message_sync_ = true;
                });
            }
            ui_.bufferView().clearReactionClick();
        }

        // thread click -> open thread panel for that message
        {
            auto tc = ui_.bufferView().lastThreadClick();
            if (tc.clicked && client_ && !active_channel_.empty()) {
                ui_.threadPanel().open(active_channel_, tc.thread_ts);
                pool_->enqueue([this, ch = active_channel_, ts = tc.thread_ts]() {
                    auto replies = client_->getThreadReplies(ch, ts);
                    ui_.threadPanel().setMessages(replies);
                });
            }
            ui_.bufferView().clearThreadClick();
        }

        // image click -> open full-size preview
        auto ic = ui_.bufferView().lastImageClick();
        if (ic.clicked && !ic.url.empty()) {
            auto tex_info = image_renderer_.getTexture(ic.url);
            ui::FilePreview::TextureInfo fp_tex;
            fp_tex.texture_id = tex_info.texture_id;
            fp_tex.width = tex_info.width;
            fp_tex.height = tex_info.height;
            ui_.filePreview().open(ic.url, fp_tex);
            ui_.bufferView().clearImageClick();
        }

        // check if input was submitted (also processed during render)
        if (ui_.inputBar().submitted()) {
            std::string text = ui_.inputBar().getText();
            handleInputSubmit(text);
            ui_.inputBar().clear();
        }

        // check if search panel has a pending query
        std::string search_query = ui_.searchPanel().pendingQuery();
        if (!search_query.empty() && client_) {
            pool_->enqueue([this, query = search_query]() {
                auto results = client_->searchMessages(query);
                std::vector<slack::Message> msgs;
                for (auto& r : results) msgs.push_back(r.message);
                ui_.searchPanel().setResults(msgs);
            });
        }

        ImGui::Render();
        int display_w, display_h;
        SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        auto& bg = ui_.theme().bg_main;
        glClearColor(bg.x, bg.y, bg.z, bg.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);

        // update window title with unread count
        if (client_) {
            int unreads = 0;
            for (auto& ch : client_->channelCache().getJoined()) {
                unreads += ch.unread_count;
            }
            std::string title = unreads > 0
                ? "Conduit [" + std::to_string(unreads) + "]"
                : "Conduit";
            SDL_SetWindowTitle(window_, title.c_str());
        }
    }
}

// ---- Input handling ----

void Application::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // ctrl+v image paste
        // catch ctrl+v for image paste
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_v &&
            (event.key.keysym.mod & KMOD_CTRL)) {
            if (tryPasteClipboardImage()) {
                continue;
            }
        }

        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
        case SDL_QUIT:
            running_ = false;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                running_ = false;
            break;
        case SDL_KEYDOWN:
            handleKeyDown(event.key);
            break;
        case SDL_DROPFILE: {
            char* file = event.drop.file;
            if (file && client_ && !active_channel_.empty()) {
                std::string path(file);
                pool_->enqueue([this, ch = active_channel_, p = path]() {
                    client_->uploadFile(ch, p);
                });
            }
            SDL_free(event.drop.file);
            break;
        }
        }
    }
}

void Application::handleKeyDown(const SDL_KeyboardEvent& key) {
    // ctrl+/- to zoom UI, ctrl+0 to reset. like every electron app ever.
    if (key.keysym.mod & KMOD_CTRL) {
        if (key.keysym.sym == SDLK_EQUALS || key.keysym.sym == SDLK_PLUS) {
            ui_scale_ = std::min(ui_scale_ + 0.1f, 3.0f);
            fonts_need_rebuild_ = true;
            return;
        }
        if (key.keysym.sym == SDLK_MINUS) {
            ui_scale_ = std::max(ui_scale_ - 0.1f, 0.5f);
            fonts_need_rebuild_ = true;
            return;
        }
        if (key.keysym.sym == SDLK_0) {
            ui_scale_ = 1.0f;
            fonts_need_rebuild_ = true;
            return;
        }
    }

    if (keys_.handleKeyDown(key)) return;
}

void Application::rebuildFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    float font_size = std::round(14.0f * ui_scale_); // round to whole pixels for crispness
    std::string exe_dir = getExeDir();

    // primary font: pixel-perfect, no blurry oversampling
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 1; // no horizontal oversampling = sharp text
    font_cfg.OversampleV = 1;
    font_cfg.PixelSnapH = true; // snap glyphs to pixel grid

    std::vector<std::string> search_paths = {
        exe_dir + "/assets/fonts/LektonNerdFontMono-Regular.ttf",
        exe_dir + "/../assets/fonts/LektonNerdFontMono-Regular.ttf",
        "assets/fonts/LektonNerdFontMono-Regular.ttf",
    };

    bool loaded = false;
    for (auto& path : search_paths) {
        if (std::filesystem::exists(path)) {
            io.Fonts->AddFontFromFileTTF(path.c_str(), font_size, &font_cfg);
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        font_cfg.SizePixels = font_size;
        io.Fonts->AddFontDefault(&font_cfg);
    }

    // emoji fallback font
    ImFontConfig emoji_cfg;
    emoji_cfg.MergeMode = true;
    emoji_cfg.OversampleH = 1;
    emoji_cfg.OversampleV = 1;
    emoji_cfg.PixelSnapH = true;
    static const ImWchar emoji_ranges[] = {
        0x2600, 0x27BF, 0x2B50, 0x2B55, 0xFE00, 0xFE0F,
        0x1F300, 0x1F9FF, 0,
    };
#ifdef _WIN32
    std::string emoji_font = "C:\\Windows\\Fonts\\seguiemj.ttf";
#else
    std::string emoji_font = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
#endif
    if (std::filesystem::exists(emoji_font)) {
        io.Fonts->AddFontFromFileTTF(emoji_font.c_str(), font_size, &emoji_cfg, emoji_ranges);
    }

    io.Fonts->Build();
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_CreateDeviceObjects();

    // reset style to defaults, THEN apply our theme, THEN scale.
    // ScaleAllSizes is cumulative so we have to start from scratch each time.
    ImGui::GetStyle() = ImGuiStyle(); // reset to imgui defaults
    ui_.theme().apply();              // apply our weechat theme
    ImGui::GetStyle().ScaleAllSizes(ui_scale_); // scale everything

    LOG_INFO("UI scale: " + std::to_string(ui_scale_) + " font: " + std::to_string(font_size) + "px");
}

bool Application::tryPasteClipboardImage() {
#ifdef _WIN32
    if (!client_ || active_channel_.empty()) return false;
    if (!OpenClipboard(NULL)) return false;

    if (!IsClipboardFormatAvailable(CF_DIB)) {
        CloseClipboard();
        return false;
    }

    HANDLE hDib = GetClipboardData(CF_DIB);
    if (!hDib) { CloseClipboard(); return false; }

    auto* bmi = static_cast<BITMAPINFOHEADER*>(GlobalLock(hDib));
    if (!bmi) { CloseClipboard(); return false; }

    int width = bmi->biWidth;
    int height = std::abs(bmi->biHeight);
    bool top_down = (bmi->biHeight < 0);
    int bit_count = bmi->biBitCount;
    if (width <= 0 || height <= 0 || bit_count < 24) {
        GlobalUnlock(hDib); CloseClipboard(); return false;
    }

    int masks_size = (bmi->biCompression == BI_BITFIELDS) ? 12 : 0;
    auto* src = reinterpret_cast<uint8_t*>(bmi) + bmi->biSize + masks_size;
    int bpp = bit_count / 8;
    int stride = ((width * bpp + 3) & ~3);

    std::vector<uint8_t> rgba(width * height * 4);
    for (int y = 0; y < height; y++) {
        int sy = top_down ? y : (height - 1 - y);
        uint8_t* srow = src + sy * stride;
        uint8_t* drow = rgba.data() + y * width * 4;
        for (int x = 0; x < width; x++) {
            drow[x*4+0] = srow[x*bpp+2];
            drow[x*4+1] = srow[x*bpp+1];
            drow[x*4+2] = srow[x*bpp+0];
            drow[x*4+3] = (bpp >= 4) ? srow[x*bpp+3] : 255;
            if (drow[x*4+3] == 0) drow[x*4+3] = 255;
        }
    }

    GlobalUnlock(hDib);
    CloseClipboard();

    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string path = std::string(tmp) + "conduit_" + std::to_string(GetTickCount()) + ".png";

    if (!stbi_write_png(path.c_str(), width, height, 4, rgba.data(), width * 4)) {
        LOG_ERROR("failed to write clipboard PNG");
        return false;
    }

    LOG_INFO("clipboard image " + std::to_string(width) + "x" +
             std::to_string(height) + " -> uploading to " + active_channel_);

    pool_->enqueue([this, ch = active_channel_, p = path]() {
        client_->uploadFile(ch, p);
        std::filesystem::remove(p);
        // refresh to see our upload
        auto history = client_->getHistory(ch, 50);
        msg_cache_->store(ch, history);
        needs_message_sync_ = true;
    });
    return true;
#else
    return false;
#endif
}

void Application::handleInputSubmit(const std::string& text) {
    if (text.empty()) return;

    // token paste flow: first the xoxc- token, then the d cookie
    if (auth_state_ == AuthState::WaitingForToken) {
        std::string token = text;
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);

        if (token.find("xoxc-") != 0 && token.find("xoxp-") != 0) {
            ui_.statusBar().setConnectionState("that doesn't look like a token, try again");
            return;
        }

        pending_token_ = token;

        if (token.find("xoxp-") == 0) {
            // xoxp tokens don't need a cookie, go straight to connect
            KeychainStore::store("conduit", "user_token", token);
            auth_state_ = AuthState::None;
            OrgConfig org;
            org.name = "Slack";
            org.user_token = token;
            org.auto_connect = true;
            config_.get().orgs.push_back(org);
            config_.save(Config::defaultPath());
            connectToSlack();
        } else {
            // xoxc tokens need the d cookie too
            ui_.statusBar().setConnectionState("now paste your d cookie");
            ui_.inputBar().setChannelName("paste d= cookie");
            auth_state_ = AuthState::WaitingForCookie;
        }
        return;
    }

    if (auth_state_ == AuthState::WaitingForCookie) {
        std::string cookie = text;
        cookie.erase(0, cookie.find_first_not_of(" \t\r\n"));
        cookie.erase(cookie.find_last_not_of(" \t\r\n") + 1);
        // strip "d=" prefix if they pasted it
        if (cookie.find("d=") == 0) cookie = cookie.substr(2);

        KeychainStore::store("conduit", "user_token", pending_token_);
        KeychainStore::store("conduit", "d_cookie", cookie);

        OrgConfig org;
        org.name = "Slack";
        org.user_token = pending_token_;
        org.d_cookie = cookie;
        org.auto_connect = true;
        config_.get().orgs.push_back(org);
        config_.save(Config::defaultPath());

        auth_state_ = AuthState::None;
        pending_token_.clear();
        connectToSlack();
        return;
    }

    // if we're in edit mode, send it as an edit to the specific message
    if (!editing_ts_.empty()) {
        std::string ts = editing_ts_;
        editing_ts_.clear();
        pool_->enqueue([this, ch = active_channel_, ts, text]() {
            client_->editMessage(ch, ts, text);
        });
        return;
    }

    // save to input history
    input_history_.add(active_channel_, text);

    // check if it's a slash command
    if (text[0] == '/') {
        if (!commands_.execute(text)) {
            LOG_WARN("unknown command: " + text);
        }
        return;
    }

    // it's a regular message
    if (!client_ || active_channel_.empty()) {
        LOG_WARN("not connected to any channel");
        return;
    }

    std::string processed = convertMentions(text);
    pool_->enqueue([this, channel = active_channel_, msg = processed]() {
        if (!client_->sendMessage(channel, msg)) {
            LOG_ERROR("failed to send message");
        }
    });
}

// ---- Slack event processing ----

void Application::processSlackEvents() {
    if (!client_) return;

    slack::SlackEvent event;
    while (client_->pollEvent(event)) {
        switch (event.type) {
        case slack::SlackEvent::Type::MessageNew:
            if (event.message) {
                // thread-only replies should not appear in the main channel.
                // a message is a thread reply when thread_ts is set and differs
                // from the message's own ts. broadcast replies (subtype
                // "thread_broadcast") DO belong in the main channel.
                bool is_thread_reply = !event.message->thread_ts.empty()
                                       && event.message->thread_ts != event.message->ts;
                bool is_broadcast = (event.message->subtype == "thread_broadcast")
                                     || event.message->reply_broadcast;

                if (!is_thread_reply || is_broadcast) {
                    msg_cache_->store(event.channel, *event.message);
                    if (event.channel == active_channel_) {
                        needs_message_sync_ = true;
                    }
                    // update unread count for other channels
                    if (event.channel != active_channel_) {
                        client_->channelCache().updateUnreadCount(
                            event.channel,
                            client_->channelCache().get(event.channel)
                                .value_or(slack::Channel{}).unread_count + 1);
                        needs_channel_sync_ = true;
                    }
                }

                // feed thread panel regardless — it wants all thread messages
                if (ui_.threadPanel().isOpen() && event.message->thread_ts == ui_.threadPanel().parentTs()) {
                    ui_.threadPanel().addMessage(*event.message);
                }

                // desktop notification for mentions and DMs (even in threads)
                if (event.channel != active_channel_) {
                    bool is_mention = event.message->text.find("<@" + client_->selfUserId() + ">") != std::string::npos;
                    auto ch_info = client_->channelCache().get(event.channel);
                    bool is_dm = ch_info && (ch_info->type == slack::ChannelType::DirectMessage);
                    if (is_mention || is_dm) {
                        std::string title = ch_info ? ch_info->name : event.channel;
                        std::string preview = event.message->text.substr(0, 80);
                        notifications_.notify(title, client_->displayName(event.message->user) + ": " + preview);
                    }
                }
            }
            break;

        case slack::SlackEvent::Type::MessageChanged:
            if (event.message) {
                msg_cache_->update(event.channel, *event.message);
                if (event.channel == active_channel_) needs_message_sync_ = true;
            }
            break;

        case slack::SlackEvent::Type::MessageDeleted:
            msg_cache_->remove(event.channel, event.ts);
            if (event.channel == active_channel_) needs_message_sync_ = true;
            break;

        case slack::SlackEvent::Type::ReactionAdded:
            if (event.reaction) {
                msg_cache_->addReaction(event.channel, event.ts, *event.reaction);
                if (event.channel == active_channel_) needs_message_sync_ = true;
            }
            break;

        case slack::SlackEvent::Type::ReactionRemoved:
            if (event.reaction) {
                msg_cache_->removeReaction(event.channel, event.ts,
                                           event.reaction->emoji_name, event.user);
                if (event.channel == active_channel_) needs_message_sync_ = true;
            }
            break;

        case slack::SlackEvent::Type::ChannelMarked:
            client_->channelCache().updateUnreadCount(event.channel, 0);
            needs_channel_sync_ = true;
            break;

        case slack::SlackEvent::Type::PresenceChange:
            if (event.is_online.has_value()) {
                client_->userCache().setOnline(event.user, *event.is_online);
                // refresh nick list if it's someone in the current channel
                needs_message_sync_ = true;
            }
            break;

        case slack::SlackEvent::Type::ChannelCreated:
        case slack::SlackEvent::Type::ChannelRenamed:
        case slack::SlackEvent::Type::ChannelArchived:
        case slack::SlackEvent::Type::ChannelJoined:
        case slack::SlackEvent::Type::ChannelLeft:
            needs_channel_sync_ = true;
            break;

        case slack::SlackEvent::Type::UserChange:
        case slack::SlackEvent::Type::TeamJoin:
            if (event.user_info) {
                client_->userCache().upsert(*event.user_info);
            }
            break;

        case slack::SlackEvent::Type::ConnectionStatus:
            if (event.connection_state) {
                ui_.statusBar().setConnectionState(*event.connection_state);
                if (*event.connection_state == "connected") {
                    needs_channel_sync_ = true;
                }
            }
            break;

        case slack::SlackEvent::Type::TypingStart: {
            if (client_ && event.user != client_->selfUserId()) {
                std::string name = client_->displayName(event.user);
                auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                bool found = false;
                for (auto& te : typing_events_) {
                    if (te.first == name) { te.second = expiry; found = true; break; }
                }
                if (!found) typing_events_.push_back({name, expiry});
            }
            break;
        }

        default:
            break;
        }
    }
}

// ---- UI sync with real data ----

void Application::syncBufferList() {
    if (!client_) return;

    auto channels = client_->channelCache().getJoined();
    std::vector<ui::BufferEntry> entries;

    // org header
    entries.push_back({client_->teamName(), false, false, 0, false, true, false, ""});

    // separate channels, DMs, and app/bot conversations
    std::vector<slack::Channel> chan_list, dm_list, app_list;
    for (auto& ch : channels) {
        bool is_dm = (ch.type == slack::ChannelType::DirectMessage ||
                      ch.type == slack::ChannelType::MultiPartyDM);
        if (!is_dm) {
            chan_list.push_back(ch);
            continue;
        }
        // check if the DM counterpart is a bot/app
        bool is_bot_dm = false;
        if (!ch.dm_user_id.empty()) {
            auto user = client_->userCache().get(ch.dm_user_id);
            if (user && user->is_bot) is_bot_dm = true;
            // slackbot has a special user ID "USLACKBOT"
            if (ch.dm_user_id == "USLACKBOT") is_bot_dm = true;
        }
        if (is_bot_dm) app_list.push_back(ch);
        else dm_list.push_back(ch);
    }

    // Channels section
    entries.push_back({"Channels", false, false, 0, false, false, true, ""});
    bool found_active = false;
    for (auto& ch : chan_list) {
        bool is_active = (ch.id == active_channel_);
        if (is_active) found_active = true;
        entries.push_back({
            "#" + ch.name, is_active, ch.has_unreads, ch.unread_count,
            false, false, false, ch.id
        });
    }

    // Direct Messages section
    entries.push_back({"Direct Messages", false, false, 0, false, false, true, ""});
    for (auto& ch : dm_list) {
        std::string display = ch.name;
        if (!ch.dm_user_id.empty()) {
            display = client_->displayName(ch.dm_user_id);
            // mark self-DM
            if (ch.dm_user_id == client_->selfUserId()) {
                display += " (you)";
            }
        }
        bool is_active = (ch.id == active_channel_);
        if (is_active) found_active = true;
        entries.push_back({
            "@" + display, is_active, ch.has_unreads, ch.unread_count,
            true, false, false, ch.id
        });
    }

    // Apps section (bot/app conversations)
    if (!app_list.empty()) {
        entries.push_back({"Apps", false, false, 0, false, false, true, ""});
        for (auto& ch : app_list) {
            std::string display = ch.name;
            if (!ch.dm_user_id.empty()) {
                display = client_->displayName(ch.dm_user_id);
            }
            bool is_active = (ch.id == active_channel_);
            if (is_active) found_active = true;
            entries.push_back({
                "@" + display, is_active, ch.has_unreads, ch.unread_count,
                true, false, false, ch.id
            });
        }
    }

    ui_.bufferList().setEntries(entries);

    // if we don't have an active channel yet, pick the first real channel
    if (!found_active) {
        for (int i = 0; i < (int)entries.size(); i++) {
            if (!entries[i].is_separator && !entries[i].is_section_header
                && !entries[i].channel_id.empty()) {
                ui_.bufferList().select(i);
                switchToBuffer(i);
                break;
            }
        }
    }
}

void Application::syncBufferView() {
    if (!client_ || active_channel_.empty()) return;

    // read from cache first — socket mode events already put messages there.
    // only hit the API if the cache is empty (first load of a channel).
    auto messages = msg_cache_->get(active_channel_, 200);
    if (messages.empty()) {
        // first time viewing this channel, fetch from API
        auto history = client_->getHistory(active_channel_, 100);
        if (!history.empty()) {
            msg_cache_->store(active_channel_, history);
            messages = msg_cache_->get(active_channel_, 200);
        }
    }

    // convert to the format BufferView wants
    std::vector<ui::BufferViewMessage> view_msgs;
    for (auto& msg : messages) {
        ui::BufferViewMessage vm;
        vm.timestamp = util::formatTime(msg.ts);
        vm.nick = client_->displayName(msg.user);
        vm.user_id = msg.user;

        // resolve <@USER_ID> mentions to <@USER_ID|displayname> so the
        // mrkdwn parser can show real names instead of raw IDs
        vm.text = msg.text;
        {
            size_t pos = 0;
            while ((pos = vm.text.find("<@", pos)) != std::string::npos) {
                size_t end = vm.text.find('>', pos);
                if (end == std::string::npos) break;
                std::string inner = vm.text.substr(pos + 2, end - pos - 2);
                if (inner.find('|') == std::string::npos) {
                    // no display name yet — resolve it
                    std::string name = client_->displayName(inner);
                    if (name != inner) {
                        vm.text.replace(pos + 2, inner.size(), inner + "|" + name);
                        end = vm.text.find('>', pos); // recalc
                    }
                }
                pos = end + 1;
            }
        }
        vm.subtype = msg.subtype;
        vm.reactions = msg.reactions;
        vm.files = msg.files;
        vm.thread_ts = msg.thread_ts;
        vm.reply_count = msg.reply_count;
        vm.is_edited = msg.is_edited;
        vm.ts = msg.ts;

        // for broadcast replies, find the parent message to show as a quote
        if ((msg.subtype == "thread_broadcast" || msg.reply_broadcast) &&
            !msg.thread_ts.empty() && msg.thread_ts != msg.ts) {
            for (auto& parent : messages) {
                if (parent.ts == msg.thread_ts) {
                    vm.reply_parent_nick = client_->displayName(parent.user);
                    vm.reply_parent_text = parent.text;
                    break;
                }
            }
        }

        // queue image/gif downloads so they're ready by the time we render
        if (!msg.files.empty()) {
            LOG_DEBUG("msg has " + std::to_string(msg.files.size()) + " files, first: " +
                      msg.files[0].name + " (" + msg.files[0].mimetype + ")");
        }
        for (auto& f : msg.files) {
            if (f.mimetype.find("image/") == 0) {
                std::string img_url = f.thumb_360.empty() ? f.url_private : f.thumb_360;
                if (!img_url.empty()) {
                    if (f.mimetype == "image/gif") {
                        gif_renderer_.requestGif(img_url, resolved_token_, *pool_);
                    } else {
                        image_renderer_.requestImage(img_url, resolved_token_, *pool_);
                    }
                }
            }
        }

        view_msgs.push_back(std::move(vm));
    }

    ui_.bufferView().setMessages(view_msgs);

    // update title bar with channel info
    auto ch_info = client_->channelCache().get(active_channel_);
    if (ch_info) {
        ui_.titleBar().setChannelName(
            ch_info->type == slack::ChannelType::DirectMessage
                ? "@" + client_->displayName(ch_info->dm_user_id)
                : "#" + ch_info->name);
        ui_.titleBar().setTopic(ch_info->topic);
        ui_.titleBar().setMemberCount(ch_info->member_count);
    }

    // mark as read
    if (!messages.empty()) {
        client_->markRead(active_channel_, messages.back().ts);
    }
}

void Application::syncNickList() {
    if (!client_ || active_channel_.empty()) return;

    auto ch_info = client_->channelCache().get(active_channel_);
    if (!ch_info) return;

    // for DMs, just show the other person
    // for channels, show all members we know about
    std::vector<ui::NickEntry> nicks;
    auto all_users = client_->userCache().getAll();

    // we don't have per-channel member lists cached yet, so show all known users
    // (good enough for now - proper member list would need conversations.members API call)
    for (auto& u : all_users) {
        if (u.id == client_->selfUserId()) continue; // don't show ourselves at the top
        nicks.push_back({u.effectiveName(), u.is_online, u.is_bot});
    }

    // sort: online first, then alphabetical
    std::sort(nicks.begin(), nicks.end(), [](const ui::NickEntry& a, const ui::NickEntry& b) {
        if (a.is_online != b.is_online) return a.is_online;
        return a.name < b.name;
    });

    ui_.nickList().setNicks(nicks);
}

void Application::switchToBuffer(int index) {
    auto& entries = ui_.bufferList().entries();
    if (index < 0 || index >= (int)entries.size()) return;
    if (entries[index].is_separator) return;

    active_channel_ = entries[index].channel_id;
    active_channel_name_ = entries[index].name;

    ui_.inputBar().setChannelName(entries[index].name);
    ui_.inputBar().setChannelId(active_channel_);

    // show cached messages immediately, fetch fresh from API in background
    needs_message_sync_ = true;

    // background fetch so the UI doesn't freeze on channel switch
    pool_->enqueue([this, ch = active_channel_]() {
        auto history = client_->getHistory(ch, 100);
        if (!history.empty()) {
            msg_cache_->store(ch, history);
            needs_message_sync_ = true; // refresh with the fresh data
        }
    });
}

// convert @displayname mentions to <@USER_ID> format so Slack treats them
// as real mentions with notifications and highlights
std::string Application::convertMentions(const std::string& text) {
    if (!client_ || text.find('@') == std::string::npos) return text;

    std::string result = text;
    // sort users by name length descending so "john smith" matches before "john"
    auto users = client_->userCache().getAll();
    std::sort(users.begin(), users.end(), [](const slack::User& a, const slack::User& b) {
        return a.effectiveName().size() > b.effectiveName().size();
    });

    for (auto& u : users) {
        std::string at_name = "@" + u.effectiveName();
        size_t pos = 0;
        while ((pos = result.find(at_name, pos)) != std::string::npos) {
            bool at_start = (pos == 0 || result[pos - 1] == ' ' || result[pos - 1] == '\n');
            size_t end = pos + at_name.size();
            bool at_end = (end >= result.size() || result[end] == ' ' ||
                           result[end] == '\n' || result[end] == ',' || result[end] == '.');
            if (at_start && at_end) {
                std::string mention = "<@" + u.id + ">";
                result.replace(pos, at_name.size(), mention);
                pos += mention.size();
            } else {
                pos += at_name.size();
            }
        }
    }
    return result;
}

// ---- Shutdown ----

void Application::shutdown() {
    // save window position/size so we can restore it next time
    if (window_) {
        std::string win_state_path = platform::getConfigDir() + "/window.state";
        platform::ensureDir(platform::getConfigDir());
        std::ofstream wf(win_state_path);
        if (wf.is_open()) {
            Uint32 flags = SDL_GetWindowFlags(window_);
            bool is_max = (flags & SDL_WINDOW_MAXIMIZED) != 0;
            int wx, wy, ww, wh;
            // get the restored (non-maximized) position and size
            if (is_max) {
                // SDL doesn't give us the restored size while maximized,
                // so save what we have and the maximized flag
                SDL_GetWindowPosition(window_, &wx, &wy);
                SDL_GetWindowSize(window_, &ww, &wh);
            } else {
                SDL_GetWindowPosition(window_, &wx, &wy);
                SDL_GetWindowSize(window_, &ww, &wh);
            }
            wf << wx << " " << wy << " " << ww << " " << wh << " " << is_max
               << " " << ui_.layout().buffer_list_width
               << " " << ui_.layout().nick_list_width;
        }
    }

    if (msg_cache_) msg_cache_->flushAll();

    if (client_) {
        client_->disconnect();
        client_.reset();
    }

    pool_.reset(); // join all threads

    if (gl_context_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    LOG_INFO("shutdown complete");
}

} // namespace conduit
