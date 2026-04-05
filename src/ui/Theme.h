#pragma once
#include <imgui.h>
#include <array>
#include <string>

namespace conduit::ui {

// pure black terminal aesthetic. if it doesn't look like you're ssh'd
// into a box in a datacenter somewhere, we've failed.
struct Theme {
    // backgrounds - black. actually black. not "dark grey pretending to be black"
    ImVec4 bg_main       = {0.00f, 0.00f, 0.00f, 1.0f};  // #000000
    ImVec4 bg_sidebar    = {0.04f, 0.04f, 0.04f, 1.0f};  // barely not black
    ImVec4 bg_input      = {0.02f, 0.02f, 0.02f, 1.0f};  // basically black
    ImVec4 bg_status     = {0.06f, 0.06f, 0.06f, 1.0f};  // status bars get a hair lighter
    ImVec4 bg_selected   = {0.12f, 0.12f, 0.18f, 1.0f};  // subtle blue tint on selection

    // text - green-ish grey default, like a worn-out CRT
    ImVec4 text_default  = {0.75f, 0.78f, 0.73f, 1.0f};  // slightly warm grey-green
    ImVec4 text_dim      = {0.40f, 0.42f, 0.38f, 1.0f};  // for timestamps and noise
    ImVec4 text_bright   = {0.95f, 0.97f, 0.93f, 1.0f};  // when something matters

    // nick colors - classic IRC vibes, saturated on black background
    std::array<ImVec4, 16> nick_colors = {{
        {0.33f, 0.80f, 0.95f, 1.0f},  // cyan
        {0.40f, 0.90f, 0.40f, 1.0f},  // green
        {0.95f, 0.45f, 0.45f, 1.0f},  // red
        {0.95f, 0.85f, 0.30f, 1.0f},  // yellow
        {0.70f, 0.50f, 0.95f, 1.0f},  // purple
        {0.95f, 0.60f, 0.20f, 1.0f},  // orange
        {0.30f, 0.90f, 0.75f, 1.0f},  // teal
        {0.95f, 0.45f, 0.70f, 1.0f},  // pink
        {0.50f, 0.75f, 0.95f, 1.0f},  // light blue
        {0.70f, 0.95f, 0.40f, 1.0f},  // lime
        {0.95f, 0.65f, 0.65f, 1.0f},  // salmon
        {0.65f, 0.65f, 0.95f, 1.0f},  // lavender
        {0.95f, 0.80f, 0.40f, 1.0f},  // gold
        {0.40f, 0.95f, 0.65f, 1.0f},  // mint
        {0.95f, 0.50f, 0.95f, 1.0f},  // magenta
        {0.55f, 0.85f, 0.85f, 1.0f},  // pale cyan
    }};

    // accents
    ImVec4 unread_indicator = {0.95f, 0.95f, 0.95f, 1.0f};
    ImVec4 mention_badge    = {0.85f, 0.15f, 0.15f, 1.0f};
    ImVec4 separator_line   = {0.15f, 0.15f, 0.15f, 1.0f};  // barely visible lines
    ImVec4 url_color        = {0.35f, 0.55f, 0.95f, 1.0f};
    ImVec4 code_bg          = {0.06f, 0.06f, 0.06f, 1.0f};

    // reactions
    float reaction_badge_height = 18.0f;
    ImVec4 reaction_bg        = {0.10f, 0.10f, 0.12f, 1.0f};
    ImVec4 reaction_bg_active = {0.15f, 0.20f, 0.30f, 1.0f};
    ImVec4 reaction_text      = {0.60f, 0.62f, 0.58f, 1.0f};
    ImVec4 reaction_count     = {0.85f, 0.87f, 0.83f, 1.0f};

    ImVec4 nickColor(const std::string& user_id) const {
        size_t hash = std::hash<std::string>{}(user_id);
        return nick_colors[hash % nick_colors.size()];
    }

    void apply() const;
};

} // namespace conduit::ui
