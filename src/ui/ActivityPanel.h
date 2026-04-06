#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>
#include <functional>

namespace conduit::ui {

struct ActivityItem {
    enum class Kind { Mention, Reaction };
    Kind kind;
    slack::ChannelId channel_id;
    std::string channel_name;
    slack::UserId actor_id;
    std::string actor_name;
    std::string message_text;   // snippet
    slack::Timestamp ts;
    std::string emoji_name;     // for reactions
    int64_t timestamp = 0;      // unix time for sorting
};

class ActivityPanel {
public:
    using JumpCallback = std::function<void(const slack::ChannelId& channel,
                                             const slack::Timestamp& ts)>;

    void open() { is_open_ = true; }
    void close() { is_open_ = false; }
    void toggle() { is_open_ = !is_open_; }
    bool isOpen() const { return is_open_; }

    void addItem(const ActivityItem& item);
    void render(float x, float y, float width, float height, const Theme& theme);

    void setJumpCallback(JumpCallback cb) { jump_cb_ = std::move(cb); }

private:
    bool is_open_ = false;
    std::vector<ActivityItem> items_;       // newest first
    static constexpr size_t MAX_ITEMS = 200;
    JumpCallback jump_cb_;
};

} // namespace conduit::ui
