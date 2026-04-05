#include "app/Application.h"
#include "app/KeychainStore.h"
#include "util/Logger.h"
#include "util/Platform.h"
#include "util/TimeFormat.h"
#include "render/TextRenderer.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <SDL_syswm.h>
#include <GL/gl.h>

#include <filesystem>
#include <fstream>
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

    // give the title bar the SDL window handle so it can drag/resize
    ui_.titleBar().setWindow(window_);
    // bump title bar height to fit window control buttons
    ui_.layout().title_bar_height = 32.0f;

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

    // plain borderless window. no Win32 style hacking — that was causing
    // ghost windows, broken keyboard input, and general misery.
    // SDL handles borderless just fine, we handle resize via the hit test.
    window_ = SDL_CreateWindow(
        "Conduit",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width_, window_height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window_) {
        LOG_ERROR(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
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
    ImGuiIO& io = ImGui::GetIO();
    std::string exe_dir = getExeDir();

    std::vector<std::string> search_paths = {
        exe_dir + "/assets/fonts/JetBrainsMono-Regular.ttf",
        exe_dir + "/../assets/fonts/JetBrainsMono-Regular.ttf",
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "../assets/fonts/JetBrainsMono-Regular.ttf",
    };

    bool loaded_primary = false;
    for (auto& path : search_paths) {
        if (std::filesystem::exists(path)) {
            io.Fonts->AddFontFromFileTTF(path.c_str(), 14.0f);
            LOG_INFO("loaded JetBrains Mono from " + path);
            loaded_primary = true;
            break;
        }
    }
    if (!loaded_primary) {
        LOG_WARN("couldn't find JetBrains Mono, using imgui default");
    }

    // merge an emoji-capable font as fallback so unicode emoji just render
    // instead of showing up as tofu rectangles. each platform ships one.
    ImFontConfig emoji_cfg;
    emoji_cfg.MergeMode = true;
    emoji_cfg.OversampleH = 1;
    emoji_cfg.OversampleV = 1;

    // the ranges we care about: misc symbols, dingbats, emoticons, the works
    static const ImWchar emoji_ranges[] = {
        0x2600, 0x27BF,   // misc symbols, dingbats
        0x2B50, 0x2B55,   // stars, circles
        0xFE00, 0xFE0F,   // variation selectors
        0x1F300, 0x1F9FF,  // emoticons, symbols, flags, etc
        0,
    };

#ifdef _WIN32
    std::string emoji_font = "C:\\Windows\\Fonts\\seguiemj.ttf";
#elif defined(__APPLE__)
    std::string emoji_font = "/System/Library/Fonts/Apple Color Emoji.ttc";
#else
    std::string emoji_font = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
#endif

    if (std::filesystem::exists(emoji_font)) {
        io.Fonts->AddFontFromFileTTF(emoji_font.c_str(), 14.0f, &emoji_cfg, emoji_ranges);
        LOG_INFO("emoji font loaded from " + emoji_font);
    } else {
        LOG_WARN("no emoji font found at " + emoji_font + " - emoji will render as :text:");
    }
}

// ---- Config & Slack connection ----

void Application::loadConfig() {
    std::string config_path = Config::defaultPath();
    config_.load(config_path);
    auto& cfg = config_.get();

    // apply UI config
    ui_.layout().buffer_list_width = cfg.ui.buffer_list_width;
    ui_.layout().nick_list_width = cfg.ui.nick_list_width;
    ui_.layout().show_buffer_list = cfg.ui.show_buffer_list;
    ui_.layout().show_nick_list = cfg.ui.show_nick_list;

    LOG_INFO("config loaded");
}

void Application::connectToSlack() {
    auto& orgs = config_.get().orgs;
    if (orgs.empty()) {
        LOG_WARN("no orgs configured - add one to " + Config::defaultPath());
        ui_.statusBar().setConnectionState("no org configured");
        ui_.statusBar().setOrgName("Conduit");
        // we'll still show the placeholder UI, just no real data
        return;
    }

    auto& org = orgs[0]; // first org for now

    // try to get tokens from keychain first
    std::string user_token = org.user_token;
    std::string app_token = org.app_token;

    if (user_token.empty()) {
        auto stored = KeychainStore::retrieve("conduit", org.name + "/user_token");
        if (stored) user_token = *stored;
    }
    if (app_token.empty()) {
        auto stored = KeychainStore::retrieve("conduit", org.name + "/app_token");
        if (stored) app_token = *stored;
    }

    if (user_token.empty()) {
        LOG_WARN("no user token for " + org.name + " - need to configure tokens");
        ui_.statusBar().setConnectionState("needs token");
        awaiting_token_ = true;
        token_prompt_field_ = "user_token";
        return;
    }

    // set up the database for this org
    std::string data_dir = platform::getDataDir();
    platform::ensureDir(data_dir);
    db_ = std::make_unique<cache::Database>();
    db_->open(data_dir + "/" + org.name + ".db");

    msg_cache_ = std::make_unique<cache::MessageCache>(*db_);

    // build the org config with resolved tokens
    OrgConfig resolved = org;
    resolved.user_token = user_token;
    resolved.app_token = app_token;

    client_ = std::make_unique<slack::SlackClient>(resolved, *db_);

    // wire up renderers so BufferView can actually show pictures and dancing GIFs
    ui_.bufferView().setImageRenderer(&image_renderer_);
    ui_.bufferView().setGifRenderer(&gif_renderer_);
    ui_.bufferView().setAuthToken(user_token);

    ui_.statusBar().setOrgName(org.name);
    ui_.statusBar().setConnectionState("connecting...");

    // connect in the background so we don't block the UI
    pool_->enqueue([this, org_name = org.name]() {
        if (client_->connect()) {
            needs_channel_sync_ = true;
            LOG_INFO("connected to " + org_name);
        } else {
            LOG_ERROR("failed to connect to " + org_name);
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
        if (cmd.argv.size() < 2 || !client_) return;
        // first arg is user, rest is message
        std::string text = cmd.args.substr(cmd.args.find(' ') + 1);
        LOG_INFO("DM to " + cmd.argv[0] + ": " + text);
        // would need to resolve user to DM channel, then send
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
        for (auto& u : client_->userCache().getAll()) {
            if (u.effectiveName() == target || u.display_name == target || u.real_name == target) {
                // find the DM channel for this user
                for (auto& ch : client_->channelCache().getJoined()) {
                    if (ch.type == slack::ChannelType::DirectMessage && ch.dm_user_id == u.id) {
                        // switch to this DM
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
                LOG_WARN("no DM channel found for " + target);
                return;
            }
        }
        LOG_WARN("user not found: " + target);
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

        // push any decoded images/gifs to the GPU (GL calls must happen on main thread)
        image_renderer_.uploadPending();
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

        // ---- imgui frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ui_.render();

        // handle custom title bar window controls
        if (ui_.titleBar().wantsClose()) {
            running_ = false;
        }
        if (ui_.titleBar().wantsMinimize()) {
            SDL_MinimizeWindow(window_);
        }
        if (ui_.titleBar().wantsMaximize()) {
            Uint32 flags = SDL_GetWindowFlags(window_);
            if (flags & SDL_WINDOW_MAXIMIZED) {
                SDL_RestoreWindow(window_);
            } else {
                SDL_MaximizeWindow(window_);
            }
        }

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
    // ctrl+v: try to paste an image from clipboard.
    // we do this AFTER imgui processes the event so text paste still works.
    // if the clipboard has a bitmap (and no text), we upload it as an image.
    // ctrl+v image paste is handled in processInput() before imgui

    // let the keybinding system try
    if (keys_.handleKeyDown(key)) return;
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

    // check if we're in token prompt mode
    if (awaiting_token_) {
        // store the token
        auto& org = config_.get().orgs[0];
        if (token_prompt_field_ == "user_token") {
            KeychainStore::store("conduit", org.name + "/user_token", text);
            LOG_INFO("user token stored");
            token_prompt_field_ = "app_token";
        } else if (token_prompt_field_ == "app_token") {
            KeychainStore::store("conduit", org.name + "/app_token", text);
            LOG_INFO("app token stored, connecting...");
            awaiting_token_ = false;
            connectToSlack();
        }
        return;
    }

    pool_->enqueue([this, channel = active_channel_, msg = text]() {
        if (client_->sendMessage(channel, msg)) {
            LOG_DEBUG("sent message to " + channel);
            // re-fetch history so we see our own message immediately
            // can't rely on socket mode echoing it back fast enough
            auto history = client_->getHistory(channel, 50);
            msg_cache_->store(channel, history);
            needs_message_sync_ = true;
        } else {
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
                // feed thread panel if it's watching this thread
                if (ui_.threadPanel().isOpen() && event.message->thread_ts == ui_.threadPanel().parentTs()) {
                    ui_.threadPanel().addMessage(*event.message);
                }
                // desktop notification for mentions and DMs
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
    entries.push_back({client_->teamName(), false, false, 0, false, true, ""});

    bool found_active = false;
    for (auto& ch : channels) {
        bool is_dm = (ch.type == slack::ChannelType::DirectMessage ||
                      ch.type == slack::ChannelType::MultiPartyDM);

        std::string display = is_dm ? ch.name : ch.name;
        if (is_dm && !ch.dm_user_id.empty()) {
            display = client_->displayName(ch.dm_user_id);
        }

        bool is_active = (ch.id == active_channel_);
        if (is_active) found_active = true;

        entries.push_back({
            is_dm ? "@" + display : "#" + display,
            is_active,
            ch.has_unreads,
            ch.unread_count,
            is_dm,
            false,
            ch.id
        });
    }

    ui_.bufferList().setEntries(entries);

    // if we don't have an active channel yet, pick the first real one
    if (!found_active && entries.size() > 1) {
        ui_.bufferList().select(1); // skip the org header
        switchToBuffer(1);
    }
}

void Application::syncBufferView() {
    if (!client_ || active_channel_.empty()) return;

    // always fetch fresh from the API so we get file/attachment data
    // the cache is good for offline fallback but attachments need the
    // promotion logic in getHistory to turn slack's weird attachment
    // format into something our renderer understands
    auto history = client_->getHistory(active_channel_, 100);
    if (!history.empty()) {
        msg_cache_->store(active_channel_, history);
    }
    auto messages = msg_cache_->get(active_channel_, 200);
    if (messages.empty()) {
        messages = history; // fallback if cache is somehow empty
    }

    // convert to the format BufferView wants
    std::vector<ui::BufferViewMessage> view_msgs;
    for (auto& msg : messages) {
        ui::BufferViewMessage vm;
        vm.timestamp = util::formatTime(msg.ts);
        vm.nick = client_->displayName(msg.user);
        vm.user_id = msg.user;
        vm.text = msg.text;
        vm.subtype = msg.subtype;
        vm.reactions = msg.reactions;
        vm.files = msg.files;
        vm.thread_ts = msg.thread_ts;
        vm.reply_count = msg.reply_count;
        vm.is_edited = msg.is_edited;
        vm.ts = msg.ts;

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
                        gif_renderer_.requestGif(img_url,
                            config_.get().orgs[0].user_token, *pool_);
                    } else {
                        image_renderer_.requestImage(img_url,
                            config_.get().orgs[0].user_token, *pool_);
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

    // update the input bar prompt
    ui_.inputBar().setChannelName(entries[index].name);
    ui_.inputBar().setChannelId(active_channel_);

    // load messages for this channel
    needs_message_sync_ = true;

    LOG_DEBUG("switched to buffer: " + entries[index].name);
}

// ---- Shutdown ----

void Application::shutdown() {
    // save caches before disconnecting
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
