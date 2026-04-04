#include "app/Config.h"
#include "util/Logger.h"
#include "util/Platform.h"

#include <toml++/toml.hpp>
#include <filesystem>
#include <fstream>

using namespace std::string_literals;
namespace fs = std::filesystem;

namespace conduit {

bool Config::load(const std::string& path) {
    if (!fs::exists(path)) {
        LOG_INFO("no config file at " + path + ", using defaults");
        return false;
    }

    try {
        auto tbl = toml::parse_file(path);

        // general section
        if (auto gen = tbl["general"]) {
            config_.font_size = gen["font_size"].value_or(14.0f);
            config_.font = gen["font"].value_or("fonts/JetBrainsMono-Regular.ttf"s);
            config_.font_bold = gen["font_bold"].value_or("fonts/JetBrainsMono-Bold.ttf"s);
            config_.font_italic = gen["font_italic"].value_or("fonts/JetBrainsMono-Italic.ttf"s);
            config_.theme = gen["theme"].value_or("themes/weechat_dark.toml"s);
            config_.log_level = gen["log_level"].value_or("info"s);
            config_.log_file = gen["log_file"].value_or(""s);
        }

        // ui section
        if (auto ui = tbl["ui"]) {
            config_.ui.show_buffer_list = ui["show_buffer_list"].value_or(true);
            config_.ui.show_nick_list = ui["show_nick_list"].value_or(true);
            config_.ui.buffer_list_width = ui["buffer_list_width"].value_or(180.0f);
            config_.ui.nick_list_width = ui["nick_list_width"].value_or(160.0f);
            config_.ui.image_max_width = ui["image_max_width"].value_or(360.0f);
            config_.ui.image_max_height = ui["image_max_height"].value_or(240.0f);
            config_.ui.timestamp_format = ui["timestamp_format"].value_or("%H:%M"s);
            config_.ui.show_seconds = ui["show_seconds"].value_or(false);
            config_.ui.nick_alignment = ui["nick_alignment"].value_or("right"s);
            config_.ui.nick_max_width = ui["nick_max_width"].value_or(15);
            config_.ui.show_avatars = ui["show_avatars"].value_or(false);
            config_.ui.animate_gifs = ui["animate_gifs"].value_or(true);
            config_.ui.show_inline_images = ui["show_inline_images"].value_or(true);
            config_.ui.buffer_sort = ui["buffer_sort"].value_or("activity"s);
        }

        // notifications
        if (auto n = tbl["notifications"]) {
            config_.notify.enabled = n["enabled"].value_or(true);
            config_.notify.notify_on = n["notify_on"].value_or("mentions"s);
            config_.notify.sound_enabled = n["sound_enabled"].value_or(false);
            config_.notify.dnd_start = n["dnd_start"].value_or(""s);
            config_.notify.dnd_end = n["dnd_end"].value_or(""s);
        }

        // network
        if (auto net = tbl["network"]) {
            config_.network.connect_timeout = net["connect_timeout"].value_or(10);
            config_.network.request_timeout = net["request_timeout"].value_or(30);
            config_.network.max_reconnect_backoff = net["max_reconnect_backoff"].value_or(30);
            config_.network.proxy = net["proxy"].value_or(""s);
            config_.network.http_proxy = net["http_proxy"].value_or(""s);
        }

        // cache
        if (auto c = tbl["cache"]) {
            config_.cache.max_file_cache_mb = c["max_file_cache_mb"].value_or(500);
            config_.cache.max_messages_per_channel = c["max_messages_per_channel"].value_or(10000);
            config_.cache.prune_days = c["prune_days"].value_or(0);
        }

        // orgs - the [[org]] array
        if (auto orgs = tbl["org"]; orgs && orgs.is_array_of_tables()) {
            for (auto& org_tbl : *orgs.as_array()) {
                OrgConfig org;
                auto& o = *org_tbl.as_table();
                org.name = o["name"].value_or(""s);
                org.app_token = o["app_token"].value_or(""s);
                org.user_token = o["user_token"].value_or(""s);
                org.bot_token = o["bot_token"].value_or(""s);
                org.auto_connect = o["auto_connect"].value_or(true);

                if (auto arr = o["auto_open"]; arr && arr.is_array()) {
                    for (auto& ch : *arr.as_array()) {
                        if (ch.is_string()) {
                            org.auto_open.push_back(std::string(ch.as_string()->get()));
                        }
                    }
                }

                config_.orgs.push_back(std::move(org));
            }
        }

        LOG_INFO("config loaded from " + path);
        return true;
    } catch (const toml::parse_error& e) {
        LOG_ERROR(std::string("config parse error: ") + e.what());
        return false;
    }
}

bool Config::save(const std::string& path) {
    // we don't write the full config back, just enough to be useful
    // tokens are stored in the keychain, not here
    try {
        std::ofstream f(path);
        if (!f.is_open()) return false;

        f << "[general]\n";
        f << "font_size = " << config_.font_size << "\n";
        f << "font = \"" << config_.font << "\"\n";
        f << "theme = \"" << config_.theme << "\"\n";
        f << "log_level = \"" << config_.log_level << "\"\n\n";

        f << "[ui]\n";
        f << "show_buffer_list = " << (config_.ui.show_buffer_list ? "true" : "false") << "\n";
        f << "show_nick_list = " << (config_.ui.show_nick_list ? "true" : "false") << "\n";
        f << "buffer_list_width = " << config_.ui.buffer_list_width << "\n\n";

        for (auto& org : config_.orgs) {
            f << "[[org]]\n";
            f << "name = \"" << org.name << "\"\n";
            f << "auto_connect = " << (org.auto_connect ? "true" : "false") << "\n";
            f << "auto_open = [";
            for (size_t i = 0; i < org.auto_open.size(); i++) {
                if (i > 0) f << ", ";
                f << "\"" << org.auto_open[i] << "\"";
            }
            f << "]\n\n";
        }

        LOG_INFO("config saved to " + path);
        return true;
    } catch (...) {
        LOG_ERROR("failed to save config to " + path);
        return false;
    }
}

std::string Config::defaultPath() {
    std::string dir = platform::getConfigDir();
    platform::ensureDir(dir);
    return dir + "/conduit.toml";
}

} // namespace conduit
