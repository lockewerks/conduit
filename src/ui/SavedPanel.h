#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>
#include <functional>

namespace conduit::ui {

struct SavedDisplayItem {
    slack::ChannelId channel_id;
    std::string channel_name;
    slack::Timestamp message_ts;
    std::string author_name;
    std::string text_snippet;
    int64_t date_saved = 0;
};

class SavedPanel {
public:
    using JumpCallback = std::function<void(const slack::ChannelId& channel,
                                             const slack::Timestamp& ts)>;
    using UnsaveCallback = std::function<void(const slack::ChannelId& channel,
                                               const slack::Timestamp& ts)>;

    void open() { is_open_ = true; }
    void close() { is_open_ = false; }
    void toggle() { is_open_ = !is_open_; }
    bool isOpen() const { return is_open_; }

    void setItems(const std::vector<SavedDisplayItem>& items) { items_ = items; }
    void setJumpCallback(JumpCallback cb) { jump_cb_ = std::move(cb); }
    void setUnsaveCallback(UnsaveCallback cb) { unsave_cb_ = std::move(cb); }

    void render(float x, float y, float width, float height, const Theme& theme);

private:
    bool is_open_ = false;
    std::vector<SavedDisplayItem> items_;
    JumpCallback jump_cb_;
    UnsaveCallback unsave_cb_;
};

} // namespace conduit::ui
