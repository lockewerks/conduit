#include "render/ReactionBadge.h"
#include "render/EmojiMap.h"
#include <imgui.h>

namespace conduit::render {

ReactionBadge::ClickResult ReactionBadge::render(
    const std::vector<slack::Reaction>& reactions,
    const conduit::ui::Theme& theme, float max_width,
    EmojiRenderer* emoji_renderer) {

    ClickResult result;
    if (reactions.empty()) return result;

    auto& emap = getEmojiMap();
    float cursor_start = ImGui::GetCursorPosX();
    float x = cursor_start;
    float emoji_size = ImGui::GetTextLineHeight();

    for (size_t i = 0; i < reactions.size(); i++) {
        auto& r = reactions[i];

        std::string count_str = " " + std::to_string(r.count);
        ImVec2 count_size = ImGui::CalcTextSize(count_str.c_str());
        float badge_w = emoji_size + count_size.x + 10.0f;
        float badge_h = theme.reaction_badge_height;

        if (x + badge_w > cursor_start + max_width && x > cursor_start) {
            x = cursor_start;
        }

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec4 bg = r.user_reacted ? theme.reaction_bg_active : theme.reaction_bg;

        // badge background
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, {pos.x + badge_w, pos.y + badge_h},
            ImGui::ColorConvertFloat4ToU32(bg), 3.0f);

        // render emoji as texture if available, otherwise fall back to text
        ImGui::SetCursorScreenPos({pos.x + 4, pos.y + 1});
        bool emoji_rendered = false;
        if (emoji_renderer) {
            emoji_rendered = emoji_renderer->renderInline(r.emoji_name, emoji_size);
        }
        if (!emoji_rendered) {
            // text fallback
            auto it = emap.find(r.emoji_name);
            std::string glyph = (it != emap.end()) ? it->second : (":" + r.emoji_name + ":");
            ImGui::PushStyleColor(ImGuiCol_Text, theme.reaction_count);
            ImGui::TextUnformatted(glyph.c_str());
            ImGui::PopStyleColor();
        }

        // count text next to the emoji
        ImGui::SameLine(0, 2);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.reaction_count);
        ImGui::TextUnformatted(count_str.c_str());
        ImGui::PopStyleColor();

        // invisible button for click handling
        ImGui::SetCursorScreenPos(pos);
        std::string btn_id = "##react_" + std::to_string(i);
        if (ImGui::InvisibleButton(btn_id.c_str(), {badge_w, badge_h})) {
            result.clicked = true;
            result.emoji_name = r.emoji_name;
        }

        // hover glow + tooltip
        if (ImGui::IsItemHovered()) {
            ImVec4 glow = bg;
            glow.x = std::min(glow.x + 0.08f, 1.0f);
            glow.y = std::min(glow.y + 0.08f, 1.0f);
            glow.z = std::min(glow.z + 0.10f, 1.0f);
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, {pos.x + badge_w, pos.y + badge_h},
                ImGui::ColorConvertFloat4ToU32(glow), 3.0f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip(":%s:", r.emoji_name.c_str());
        }

        ImGui::SameLine(0, 4);
        x += badge_w + 4;
    }

    ImGui::NewLine();
    return result;
}

} // namespace conduit::render
