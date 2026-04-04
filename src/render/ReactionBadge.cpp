#include "render/ReactionBadge.h"
#include <imgui.h>

namespace conduit::render {

ReactionBadge::ClickResult ReactionBadge::render(
    const std::vector<slack::Reaction>& reactions,
    const conduit::ui::Theme& theme, float max_width) {

    ClickResult result;
    if (reactions.empty()) return result;

    float cursor_start = ImGui::GetCursorPosX();
    float x = cursor_start;

    for (size_t i = 0; i < reactions.size(); i++) {
        auto& r = reactions[i];
        std::string label = ":" + r.emoji_name + ": " + std::to_string(r.count);
        ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        float badge_w = text_size.x + 12.0f;
        float badge_h = theme.reaction_badge_height;

        // wrap to next line if needed
        if (x + badge_w > cursor_start + max_width && x > cursor_start) {
            x = cursor_start;
            // new line - ImGui handles this via cursor position
        }

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec4 bg = r.user_reacted ? theme.reaction_bg_active : theme.reaction_bg;

        // draw the badge background
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, {pos.x + badge_w, pos.y + badge_h},
            ImGui::ColorConvertFloat4ToU32(bg), 3.0f);

        // badge text
        ImGui::SetCursorScreenPos({pos.x + 6, pos.y + 1});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.reaction_count);
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopStyleColor();

        // invisible button for click handling
        ImGui::SetCursorScreenPos(pos);
        std::string btn_id = "##react_" + std::to_string(i);
        if (ImGui::InvisibleButton(btn_id.c_str(), {badge_w, badge_h})) {
            result.clicked = true;
            result.emoji_name = r.emoji_name;
        }

        ImGui::SameLine(0, 4);
        x += badge_w + 4;
    }

    // reset to left after reaction row
    ImGui::NewLine();

    return result;
}

} // namespace conduit::render
