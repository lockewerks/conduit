#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>

namespace conduit::ui {

// side panel for viewing thread replies
// opens to the right of the main buffer view
class ThreadPanel {
public:
    void open(const slack::ChannelId& channel, const slack::Timestamp& parent_ts);
    void close();
    bool isOpen() const { return is_open_; }

    void setMessages(const std::vector<slack::Message>& messages) { messages_ = messages; }
    void addMessage(const slack::Message& message);
    void render(float x, float y, float width, float height, const Theme& theme);

    slack::ChannelId channelId() const { return channel_id_; }
    slack::Timestamp parentTs() const { return parent_ts_; }

private:
    bool is_open_ = false;
    slack::ChannelId channel_id_;
    slack::Timestamp parent_ts_;
    std::vector<slack::Message> messages_;
};

} // namespace conduit::ui
