#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>

namespace conduit::render {

// renders the [:emoji: N] reaction badges below messages
class ReactionBadge {
public:
    // render a row of reaction badges
    // returns true if any badge was clicked (for toggle)
    struct ClickResult {
        bool clicked = false;
        std::string emoji_name;
    };

    static ClickResult render(const std::vector<slack::Reaction>& reactions,
                               const conduit::ui::Theme& theme,
                               float max_width);
};

} // namespace conduit::render
