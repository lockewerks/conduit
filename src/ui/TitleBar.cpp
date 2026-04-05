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

    // channel name + topic, weechat style: "[#channel] topic text here"
    ImGui::SetCursorPos({x + 4.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    std::string header = "[" + channel_name_ + "]";
    if (member_count_ > 0) header += "(" + std::to_string(member_count_) + ")";
    ImGui::TextUnformatted(header.c_str());
    ImGui::PopStyleColor();

    if (!topic_.empty()) {
        ImGui::SameLine(0, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::PushClipRect({p.x, p.y}, {p.x + width, p.y + height}, true);
        ImGui::TextUnformatted(topic_.c_str());
        ImGui::PopClipRect();
        ImGui::PopStyleColor();
    }
}

} // namespace conduit::ui
