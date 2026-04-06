#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include "render/EmojiRenderer.h"
#include <vector>

namespace conduit::render {

class ReactionBadge {
public:
    struct ClickResult {
        bool clicked = false;
        std::string emoji_name;
    };

    static ClickResult render(const std::vector<slack::Reaction>& reactions,
                               const conduit::ui::Theme& theme,
                               float max_width,
                               EmojiRenderer* emoji_renderer = nullptr);
};

} // namespace conduit::render
