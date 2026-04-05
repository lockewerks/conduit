#include "ui/TitleBar.h"
#include <imgui.h>

namespace conduit::ui {

void TitleBar::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // dark strip across the top of the chat area
    dl->AddRectFilled(p, {p.x + width, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.bg_status));

    // thin bottom border so it doesn't just float there
    dl->AddLine({p.x, p.y + height - 1.0f}, {p.x + width, p.y + height - 1.0f},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // channel name on the left, nice and bright
    ImGui::SetCursorPos({x + 10.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);

    // the channel_name_ already has '#' or '@' prefix from syncBufferView,
    // but just in case someone set it without one
    ImGui::TextUnformatted(channel_name_.c_str());
    ImGui::PopStyleColor();

    float cursor_x = ImGui::GetCursorPosX();
    float name_width = ImGui::CalcTextSize(channel_name_.c_str()).x;
    float after_name = x + 10.0f + name_width;

    // member count if we have it (DMs don't really need this)
    if (member_count_ > 0) {
        ImGui::SetCursorPos({after_name + 10.0f, text_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        std::string members = std::to_string(member_count_) + " members";
        ImGui::TextUnformatted(members.c_str());
        ImGui::PopStyleColor();
        after_name += 10.0f + ImGui::CalcTextSize(members.c_str()).x;
    }

    // vertical separator between name info and topic
    if (!topic_.empty()) {
        float sep_x = after_name + 12.0f;
        dl->AddLine({p.x + sep_x - x, p.y + 5.0f}, {p.x + sep_x - x, p.y + height - 5.0f},
                    ImGui::ColorConvertFloat4ToU32(theme.separator_line));

        // topic text, dimmed, with wrapping off so it just truncates
        ImGui::SetCursorPos({sep_x + 10.0f, text_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(topic_.c_str());
        ImGui::PopStyleColor();
    }
}

} // namespace conduit::ui
