#pragma once
#include <string>
#include <vector>
#include "slack/Types.h"
#include "ui/Theme.h"
#include "render/ImageRenderer.h"
#include "render/GifRenderer.h"

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

    // renderers for inline content
    void setImageRenderer(render::ImageRenderer* r) { image_renderer_ = r; }
    void setGifRenderer(render::GifRenderer* r) { gif_renderer_ = r; }
    void setAuthToken(const std::string& t) { auth_token_ = t; }

    // message selection (for reactions, edit, delete, thread)
    int selectedIndex() const { return selected_index_; }
    const std::string& selectedTs() const { return selected_ts_; }
    void clearSelection() { selected_index_ = -1; selected_ts_.clear(); }

    // reaction click feedback - someone clicked a badge and we need to tell the app
    struct ReactionClick {
        bool clicked = false;
        std::string emoji_name;
        std::string message_ts;
    };
    ReactionClick lastReactionClick() const { return last_reaction_click_; }
    void clearReactionClick() { last_reaction_click_ = {}; }

    // image click feedback - user clicked an inline image to view it full size
    struct ImageClick {
        bool clicked = false;
        std::string url;
    };
    ImageClick lastImageClick() const { return last_image_click_; }
    void clearImageClick() { last_image_click_ = {}; }

private:
    std::vector<BufferViewMessage> messages_;
    bool auto_scroll_ = true;
    bool has_new_data_ = false;

    render::ImageRenderer* image_renderer_ = nullptr;
    render::GifRenderer* gif_renderer_ = nullptr;
    std::string auth_token_;
    int selected_index_ = -1;
    std::string selected_ts_;
    ReactionClick last_reaction_click_;
    ImageClick last_image_click_;
};

} // namespace conduit::ui
