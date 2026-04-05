#pragma once
#include <string>
#include <vector>
#include <optional>

namespace conduit {

struct OrgConfig {
    std::string name;
    std::string app_token;  // xapp- token for socket mode
    std::string user_token; // xoxp- token for API calls
    std::string bot_token;  // xoxb- optional
    std::string d_cookie;   // session cookie for xoxc- tokens (pilfered from Slack desktop)
    bool auto_connect = true;
    std::vector<std::string> auto_open; // channels to open on connect
};

struct UIConfig {
    bool show_buffer_list = true;
    bool show_nick_list = true;
    float buffer_list_width = 180.0f;
    float nick_list_width = 160.0f;
    float image_max_width = 360.0f;
    float image_max_height = 240.0f;
    std::string timestamp_format = "%H:%M";
    bool show_seconds = false;
    std::string nick_alignment = "right";
    int nick_max_width = 15;
    bool show_avatars = false;
    bool animate_gifs = true;
    bool show_inline_images = true;
    std::string buffer_sort = "activity";
};

struct NotifyConfig {
    bool enabled = true;
    std::string notify_on = "mentions";
    bool sound_enabled = false;
    std::string dnd_start;
    std::string dnd_end;
};

struct NetworkConfig {
    int connect_timeout = 10;
    int request_timeout = 30;
    int max_reconnect_backoff = 30;
    std::string proxy;
    std::string http_proxy;
};

struct CacheConfig {
    int max_file_cache_mb = 500;
    int max_messages_per_channel = 10000;
    int prune_days = 0;
};

struct AppConfig {
    // general
    float font_size = 14.0f;
    std::string font = "fonts/JetBrainsMono-Regular.ttf";
    std::string font_bold = "fonts/JetBrainsMono-Bold.ttf";
    std::string font_italic = "fonts/JetBrainsMono-Italic.ttf";
    std::string theme = "themes/weechat_dark.toml";
    std::string log_level = "info";
    std::string log_file;

    UIConfig ui;
    NotifyConfig notify;
    NetworkConfig network;
    CacheConfig cache;
    std::vector<OrgConfig> orgs;
};

class Config {
public:
    // load from file, returns false if file doesn't exist (uses defaults)
    bool load(const std::string& path);
    bool save(const std::string& path);

    AppConfig& get() { return config_; }
    const AppConfig& get() const { return config_; }

    // get the config file path (creates dir if needed)
    static std::string defaultPath();

private:
    AppConfig config_;
};

} // namespace conduit
