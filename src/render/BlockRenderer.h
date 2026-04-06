#pragma once
#include "slack/Types.h"
#include "render/TextRenderer.h"
#include "render/ImageRenderer.h"
#include "render/EmojiRenderer.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace conduit::render {

// callback when a user clicks a button in an actions block
struct BlockAction {
    bool clicked = false;
    std::string action_id;
    std::string block_id;
    std::string value;
};

class BlockRenderer {
public:
    void setImageRenderer(ImageRenderer* r) { image_renderer_ = r; }
    void setEmojiRenderer(EmojiRenderer* r) { emoji_renderer_ = r; }
    void setAuthToken(const std::string& t) { auth_token_ = t; }

    // render a list of blocks inline in the message view
    // returns the total height consumed
    float render(const std::vector<slack::Block>& blocks, float wrap_width,
                 const ImVec4& default_color);

    // check if a button was clicked last frame
    BlockAction lastAction() const { return last_action_; }
    void clearAction() { last_action_ = {}; }

    // resolve user/channel IDs to display names
    using NameResolver = std::function<std::string(const std::string& id)>;
    void setUserResolver(NameResolver r) { resolve_user_ = r; }
    void setChannelResolver(NameResolver r) { resolve_channel_ = r; }
    void setUserGroupResolver(NameResolver r) { resolve_usergroup_ = r; }

private:
    ImageRenderer* image_renderer_ = nullptr;
    EmojiRenderer* emoji_renderer_ = nullptr;
    std::string auth_token_;
    BlockAction last_action_;

    NameResolver resolve_user_;
    NameResolver resolve_channel_;
    NameResolver resolve_usergroup_;

    void renderRichText(const slack::Block& block, float wrap_width, const ImVec4& color);
    void renderRichTextSection(const slack::BlockElement& section, float wrap_width,
                                const ImVec4& color);
    void renderSection(const slack::Block& block, float wrap_width, const ImVec4& color);
    void renderHeader(const slack::Block& block);
    void renderDivider(float width);
    void renderContext(const slack::Block& block, float wrap_width);
    void renderImage(const slack::Block& block, float max_width);
    void renderActions(const slack::Block& block);
};

} // namespace conduit::render
