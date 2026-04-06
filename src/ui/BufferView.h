#pragma once
#include <string>
#include <vector>
#include "slack/Types.h"
#include "ui/Theme.h"
#include "render/ImageRenderer.h"
#include "render/GifRenderer.h"
#include "render/EmojiRenderer.h"

namespace conduit::ui {

struct BufferViewMessage {
    std::string timestamp;
    std::string nick;
    std::string user_id;
    std::string text;
    std::string subtype;
    std::vector<slack::Reaction> reactions;
    std::vector<slack::Attachment> attachments;
    std::vector<slack::Block> blocks;
    std::vector<slack::SlackFile> files;
    std::string thread_ts;
    int reply_count = 0;
    bool is_edited = false;
    std::string ts;
    std::string bot_id;
    bool is_saved = false;
    bool is_pinned = false;

    // for broadcast replies: the parent message we're replying to
    std::string reply_parent_nick;
    std::string reply_parent_text;
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
    void setEmojiRenderer(render::EmojiRenderer* r) { emoji_renderer_ = r; }
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

    // thread click feedback - user clicked "[N replies]" to open thread
    struct ThreadClick {
        bool clicked = false;
        std::string thread_ts;
    };
    ThreadClick lastThreadClick() const { return last_thread_click_; }
    void clearThreadClick() { last_thread_click_ = {}; }

    // image click feedback - user clicked an inline image to view it full size
    struct ImageClick {
        bool clicked = false;
        std::string url;
    };
    ImageClick lastImageClick() const { return last_image_click_; }
    void clearImageClick() { last_image_click_ = {}; }

    // right-click context menu action - whoever owns us should check this each frame
    struct ContextAction {
        enum Type { None, Copy, CopyLink, Reply, React, QuickReact, Edit, Delete,
                     Save, Unsave, Pin, Unpin, RemindMe, MarkUnread, ViewProfile } type = None;
        std::string ts;
        std::string text;
        std::string emoji;    // for QuickReact
        std::string user_id;  // for QuoteReply or ViewProfile
        int remind_minutes = 0;
    };
    ContextAction lastContextAction() const { return last_context_action_; }
    void clearContextAction() { last_context_action_ = {}; }

    // profile click feedback - user clicked a @mention or nick to view profile
    struct ProfileClick {
        bool clicked = false;
        std::string user_id;
    };
    ProfileClick lastProfileClick() const { return last_profile_click_; }
    void clearProfileClick() { last_profile_click_ = {}; }

    // tell us who we are so we can show/hide edit/delete
    void setSelfUserId(const std::string& id) { self_user_id_ = id; }

private:
    std::vector<BufferViewMessage> messages_;
    bool auto_scroll_ = true;
    bool has_new_data_ = false;

    render::ImageRenderer* image_renderer_ = nullptr;
    render::GifRenderer* gif_renderer_ = nullptr;
    render::EmojiRenderer* emoji_renderer_ = nullptr;
    std::string auth_token_;
    int selected_index_ = -1;
    std::string selected_ts_;
    ReactionClick last_reaction_click_;
    ThreadClick last_thread_click_;
    ImageClick last_image_click_;
    ContextAction last_context_action_;
    ProfileClick last_profile_click_;
    std::string context_msg_ts_;
    std::string context_msg_user_;
    std::string context_msg_text_;
    bool context_msg_saved_ = false;
    bool context_msg_pinned_ = false;
    std::string self_user_id_;
};

} // namespace conduit::ui
