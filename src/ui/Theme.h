#pragma once
#include <imgui.h>
#include <array>
#include <string>

namespace conduit::ui {

// weechat-inspired dark theme
// every color painstakingly chosen to look like you're ssh'd into something cool
struct Theme {
    // backgrounds
    ImVec4 bg_main       = {0.11f, 0.11f, 0.14f, 1.0f};
    ImVec4 bg_sidebar    = {0.09f, 0.09f, 0.12f, 1.0f};
    ImVec4 bg_input      = {0.13f, 0.13f, 0.17f, 1.0f};
    ImVec4 bg_status     = {0.07f, 0.07f, 0.10f, 1.0f};
    ImVec4 bg_selected   = {0.20f, 0.20f, 0.28f, 1.0f};

    // text
    ImVec4 text_default  = {0.80f, 0.80f, 0.80f, 1.0f};
    ImVec4 text_dim      = {0.50f, 0.50f, 0.55f, 1.0f};
    ImVec4 text_bright   = {1.00f, 1.00f, 1.00f, 1.0f};

    // nick colors (16 of them, cycled per user, just like weechat)
    std::array<ImVec4, 16> nick_colors = {{
        {0.47f, 0.78f, 1.00f, 1.0f},  // cyan
        {0.53f, 1.00f, 0.53f, 1.0f},  // green
        {1.00f, 0.60f, 0.60f, 1.0f},  // red
        {1.00f, 0.85f, 0.40f, 1.0f},  // yellow
        {0.78f, 0.58f, 1.00f, 1.0f},  // purple
        {1.00f, 0.65f, 0.30f, 1.0f},  // orange
        {0.40f, 1.00f, 0.85f, 1.0f},  // teal
        {1.00f, 0.50f, 0.80f, 1.0f},  // pink
        {0.60f, 0.85f, 1.00f, 1.0f},  // light blue
        {0.85f, 1.00f, 0.60f, 1.0f},  // lime
        {1.00f, 0.75f, 0.75f, 1.0f},  // salmon
        {0.75f, 0.75f, 1.00f, 1.0f},  // lavender
        {1.00f, 0.90f, 0.60f, 1.0f},  // gold
        {0.60f, 1.00f, 0.75f, 1.0f},  // mint
        {1.00f, 0.60f, 1.00f, 1.0f},  // magenta
        {0.70f, 0.90f, 0.90f, 1.0f},  // pale cyan
    }};

    // ui accents
    ImVec4 unread_indicator = {1.00f, 1.00f, 1.00f, 1.0f};
    ImVec4 mention_badge    = {0.90f, 0.20f, 0.20f, 1.0f};
    ImVec4 separator_line   = {0.25f, 0.25f, 0.30f, 1.0f};
    ImVec4 url_color        = {0.40f, 0.60f, 1.00f, 1.0f};
    ImVec4 code_bg          = {0.08f, 0.08f, 0.11f, 1.0f};

    // reactions
    ImVec4 reaction_bg        = {0.18f, 0.18f, 0.24f, 1.0f};
    ImVec4 reaction_bg_active = {0.22f, 0.30f, 0.45f, 1.0f};
    ImVec4 reaction_text      = {0.70f, 0.70f, 0.75f, 1.0f};
    ImVec4 reaction_count     = {0.90f, 0.90f, 0.95f, 1.0f};

    // get a deterministic nick color from a user id
    ImVec4 nickColor(const std::string& user_id) const {
        size_t hash = std::hash<std::string>{}(user_id);
        return nick_colors[hash % nick_colors.size()];
    }

    // slam the theme into imgui's style system
    void apply() const;
};

} // namespace conduit::ui
