#include "ui/TitleBar.h"
#include <imgui.h>

namespace conduit::ui {

void TitleBar::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // dark background strip across the top
    dl->AddRectFilled(p, {p.x + width, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.bg_status));

    // channel name on the left
    ImGui::SetCursorPos({x + 8.0f, y + 3.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted(channel_name_.c_str());
    ImGui::PopStyleColor();

    // topic after the channel name, dimmed
    float name_width = ImGui::CalcTextSize(channel_name_.c_str()).x;
    ImGui::SetCursorPos({x + 8.0f + name_width + 16.0f, y + 3.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted(("| " + topic_).c_str());
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
