#include "ui/TitleBar.h"
#include <imgui.h>

namespace conduit::ui {

void TitleBar::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(p, {p.x + width, p.y + height},
                       ImGui::ColorConvertFloat4ToU32(theme.bg_status));
    dl->AddLine({p.x, p.y + height - 1.0f}, {p.x + width, p.y + height - 1.0f},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // channel name
    ImGui::SetCursorPos({x + 10.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted(channel_name_.c_str());
    ImGui::PopStyleColor();

    float after = x + 10.0f + ImGui::CalcTextSize(channel_name_.c_str()).x;

    // member count
    if (member_count_ > 0) {
        ImGui::SetCursorPos({after + 10.0f, text_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        std::string members = std::to_string(member_count_) + " members";
        ImGui::TextUnformatted(members.c_str());
        ImGui::PopStyleColor();
        after += 10.0f + ImGui::CalcTextSize(members.c_str()).x;
    }

    // topic
    if (!topic_.empty()) {
        float sep_x = after + 12.0f;
        dl->AddLine({p.x + sep_x, p.y + 5.0f}, {p.x + sep_x, p.y + height - 5.0f},
                    ImGui::ColorConvertFloat4ToU32(theme.separator_line));

        ImGui::SetCursorPos({sep_x + 10.0f, text_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::PushClipRect({p.x + sep_x + 10.0f, p.y}, {p.x + width - 10.0f, p.y + height}, true);
        ImGui::TextUnformatted(topic_.c_str());
        ImGui::PopClipRect();
        ImGui::PopStyleColor();
    }
}

} // namespace conduit::ui
