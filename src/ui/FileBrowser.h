#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>
#include <functional>
#include <string>

namespace conduit::ui {

class FileBrowser {
public:
    using FetchCallback = std::function<std::vector<slack::SlackFile>(
        const slack::ChannelId& channel, int count, int page)>;

    void open(const slack::ChannelId& channel = "");
    void close() { is_open_ = false; }
    bool isOpen() const { return is_open_; }

    void setFiles(const std::vector<slack::SlackFile>& files, int total);
    void setFetchCallback(FetchCallback cb) { fetch_cb_ = std::move(cb); }

    void render(float x, float y, float width, float height, const Theme& theme);

private:
    bool is_open_ = false;
    slack::ChannelId channel_id_;
    std::vector<slack::SlackFile> files_;
    int total_files_ = 0;
    int current_page_ = 1;
    FetchCallback fetch_cb_;
    bool needs_fetch_ = false;
};

} // namespace conduit::ui
