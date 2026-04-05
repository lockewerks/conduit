#include "ui/TitleBar.h"
#include <imgui.h>

namespace conduit::ui {

void TitleBar::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // status bar style - dark strip
    dl->AddRectFilled(p, {p.x + width, p.y + height},
                       ImGui::ColorConvertFloat4ToU32(theme.bg_status));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // channel name + topic, weechat style
    ImGui::SetCursorPos({x + 4.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    std::string header = "[" + channel_name_ + "]";
    if (member_count_ > 0) header += "(" + std::to_string(member_count_) + ")";
    // truncate if wider than the bar
    float header_w = ImGui::CalcTextSize(header.c_str()).x;
    float max_header_w = width * 0.4f;
    if (header_w > max_header_w) {
        // just show the channel name without member count
        header = "[" + channel_name_ + "]";
    }
    ImGui::TextUnformatted(header.c_str());
    ImGui::PopStyleColor();

    if (!topic_.empty()) {
        ImGui::SameLine(0, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        // clip topic to remaining space
        float remaining = width - ImGui::GetCursorPosX() - 4.0f;
        if (remaining > 30.0f) {
            ImVec2 clip_min = ImGui::GetCursorScreenPos();
            ImGui::PushClipRect(clip_min, {clip_min.x + remaining, clip_min.y + height}, true);
            ImGui::TextUnformatted(topic_.c_str());
            ImGui::PopClipRect();
        }
        ImGui::PopStyleColor();
    }
}

} // namespace conduit::ui
