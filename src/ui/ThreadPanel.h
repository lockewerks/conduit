#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>
#include <string>
#include <functional>
#include <cstring>

namespace conduit::ui {

// side panel for viewing thread replies
// opens to the right of the main buffer view
class ThreadPanel {
public:
    using DisplayNameFn = std::function<std::string(const std::string& user_id)>;
    using ReplyCallback = std::function<void(const slack::ChannelId& channel,
                                             const slack::Timestamp& thread_ts,
                                             const std::string& text)>;

    void open(const slack::ChannelId& channel, const slack::Timestamp& parent_ts);
    void close();
    bool isOpen() const { return is_open_; }

    void setMessages(const std::vector<slack::Message>& messages) { messages_ = messages; }
    void addMessage(const slack::Message& message);
    void render(float x, float y, float width, float height, const Theme& theme);

    slack::ChannelId channelId() const { return channel_id_; }
    slack::Timestamp parentTs() const { return parent_ts_; }

    // set a function to resolve user IDs to display names
    void setDisplayNameFn(DisplayNameFn fn) { display_name_fn_ = std::move(fn); }

    // set a callback for when the user submits a reply
    void setReplyCallback(ReplyCallback cb) { reply_cb_ = std::move(cb); }

private:
    bool is_open_ = false;
    slack::ChannelId channel_id_;
    slack::Timestamp parent_ts_;
    std::vector<slack::Message> messages_;

    char reply_buf_[2048] = {};
    bool focus_reply_ = false;

    DisplayNameFn display_name_fn_;
    ReplyCallback reply_cb_;
};

} // namespace conduit::ui
