#pragma once
#include <string>
#include <vector>
#include "slack/Types.h"
#include "ui/Theme.h"
#include "render/ImageRenderer.h"

namespace conduit::ui {

struct BufferViewMessage {
    std::string timestamp;
    std::string nick;
    std::string user_id;
    std::string text;
    std::string subtype;
    std::vector<slack::Reaction> reactions;
    std::vector<slack::SlackFile> files;
    std::string thread_ts;
    int reply_count = 0;
    bool is_edited = false;
    std::string ts;
};

class BufferView {
public:
    BufferView();
    void render(float x, float y, float width, float height, const Theme& theme);

    void setMessages(const std::vector<BufferViewMessage>& messages);
    void scrollToBottom();
    bool isScrolledToBottom() const { return auto_scroll_; }

    // image renderer for inline images
    void setImageRenderer(render::ImageRenderer* r) { image_renderer_ = r; }
    void setAuthToken(const std::string& t) { auth_token_ = t; }

    // message selection (for reactions, edit, delete, thread)
    int selectedIndex() const { return selected_index_; }
    const std::string& selectedTs() const { return selected_ts_; }
    void clearSelection() { selected_index_ = -1; selected_ts_.clear(); }

private:
    std::vector<BufferViewMessage> messages_;
    bool auto_scroll_ = true;
    bool has_new_data_ = false;

    render::ImageRenderer* image_renderer_ = nullptr;
    std::string auth_token_;
    int selected_index_ = -1;
    std::string selected_ts_;
};

} // namespace conduit::ui
