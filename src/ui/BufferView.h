#pragma once
#include <string>
#include <vector>
#include "slack/Types.h"
#include "ui/Theme.h"

namespace conduit::ui {

// a message ready for display - already resolved from slack data
struct BufferViewMessage {
    std::string timestamp;  // formatted "HH:MM"
    std::string nick;       // display name
    std::string user_id;    // for nick color
    std::string text;       // raw mrkdwn text
    std::string subtype;
    std::vector<slack::Reaction> reactions;
    std::vector<slack::SlackFile> files;
    std::string thread_ts;
    int reply_count = 0;
    bool is_edited = false;
    std::string ts;         // raw slack timestamp
};

class BufferView {
public:
    BufferView();
    void render(float x, float y, float width, float height, const Theme& theme);

    void setMessages(const std::vector<BufferViewMessage>& messages);
    void scrollToBottom();
    bool isScrolledToBottom() const { return auto_scroll_; }

private:
    std::vector<BufferViewMessage> messages_;
    bool auto_scroll_ = true;
    bool has_new_data_ = false;
};

} // namespace conduit::ui
