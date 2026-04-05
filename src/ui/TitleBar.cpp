#include "ui/TitleBar.h"
#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace conduit::ui {

void TitleBar::render(float x, float y, float width, float height, const Theme& theme) {
    wants_close_ = false;
    wants_minimize_ = false;
    wants_maximize_ = false;

    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // title bar background - slightly different from the rest so it stands out
    ImVec4 title_bg = {theme.bg_status.x + 0.02f, theme.bg_status.y + 0.02f,
                       theme.bg_status.z + 0.03f, 1.0f};
    dl->AddRectFilled(p, {p.x + width, p.y + height},
                       ImGui::ColorConvertFloat4ToU32(title_bg));

    // bottom border
    dl->AddLine({p.x, p.y + height - 1.0f}, {p.x + width, p.y + height - 1.0f},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    float btn_size = height - 4.0f;
    float btn_y = y + 2.0f;

    // ---- window control buttons on the right (close, maximize, minimize) ----
    // laid out right-to-left because that's where windows users expect them

    float btn_x = x + width;

    // close button (red on hover, because drama)
    btn_x -= btn_size + 4.0f;
    ImGui::SetCursorPos({btn_x, btn_y});
    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.85f, 0.20f, 0.20f, 0.9f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {1.0f, 0.15f, 0.15f, 1.0f});
    if (ImGui::Button("##close", {btn_size, btn_size})) {
        wants_close_ = true;
    }
    ImGui::PopStyleColor(3);
    // draw the X
    {
        ImVec2 bp = ImGui::GetItemRectMin();
        float cx = bp.x + btn_size * 0.5f;
        float cy = bp.y + btn_size * 0.5f;
        float s = 5.0f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(theme.text_dim);
        if (ImGui::IsItemHovered()) col = ImGui::ColorConvertFloat4ToU32(theme.text_bright);
        dl->AddLine({cx - s, cy - s}, {cx + s, cy + s}, col, 1.5f);
        dl->AddLine({cx + s, cy - s}, {cx - s, cy + s}, col, 1.5f);
    }

    // maximize/restore button
    btn_x -= btn_size + 2.0f;
    ImGui::SetCursorPos({btn_x, btn_y});
    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.3f, 0.3f, 0.35f, 0.6f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.4f, 0.4f, 0.45f, 0.8f});
    if (ImGui::Button("##maximize", {btn_size, btn_size})) {
        wants_maximize_ = true;
    }
    ImGui::PopStyleColor(3);
    // draw the square
    {
        ImVec2 bp = ImGui::GetItemRectMin();
        float cx = bp.x + btn_size * 0.5f;
        float cy = bp.y + btn_size * 0.5f;
        float s = 5.0f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(theme.text_dim);
        if (ImGui::IsItemHovered()) col = ImGui::ColorConvertFloat4ToU32(theme.text_bright);
        dl->AddRect({cx - s, cy - s}, {cx + s, cy + s}, col, 0.0f, 0, 1.2f);
    }

    // minimize button
    btn_x -= btn_size + 2.0f;
    ImGui::SetCursorPos({btn_x, btn_y});
    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.3f, 0.3f, 0.35f, 0.6f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.4f, 0.4f, 0.45f, 0.8f});
    if (ImGui::Button("##minimize", {btn_size, btn_size})) {
        wants_minimize_ = true;
    }
    ImGui::PopStyleColor(3);
    // draw the dash
    {
        ImVec2 bp = ImGui::GetItemRectMin();
        float cx = bp.x + btn_size * 0.5f;
        float cy = bp.y + btn_size * 0.5f;
        float s = 5.0f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(theme.text_dim);
        if (ImGui::IsItemHovered()) col = ImGui::ColorConvertFloat4ToU32(theme.text_bright);
        dl->AddLine({cx - s, cy}, {cx + s, cy}, col, 1.5f);
    }

    // the usable width for channel info (everything left of the window buttons)
    float info_max_x = btn_x - 12.0f;

    // ---- app name on the far left ----
    ImGui::SetCursorPos({x + 10.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.nick_colors[0]); // cyan accent
    ImGui::TextUnformatted("Conduit");
    ImGui::PopStyleColor();
    float after_logo = x + 10.0f + ImGui::CalcTextSize("Conduit").x;

    // thin separator after the app name
    float sep1_x = after_logo + 10.0f;
    dl->AddLine({p.x + sep1_x, p.y + 6.0f}, {p.x + sep1_x, p.y + height - 6.0f},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    // ---- channel name ----
    float chan_x = sep1_x + 10.0f;
    ImGui::SetCursorPos({chan_x, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted(channel_name_.c_str());
    ImGui::PopStyleColor();

    float after_chan = chan_x + ImGui::CalcTextSize(channel_name_.c_str()).x;

    // member count
    if (member_count_ > 0) {
        ImGui::SetCursorPos({after_chan + 8.0f, text_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        std::string members = std::to_string(member_count_) + " members";
        ImGui::TextUnformatted(members.c_str());
        ImGui::PopStyleColor();
        after_chan += 8.0f + ImGui::CalcTextSize(members.c_str()).x;
    }

    // topic (dimmed, truncated to fit)
    if (!topic_.empty() && after_chan + 30.0f < info_max_x) {
        float sep2_x = after_chan + 10.0f;
        dl->AddLine({p.x + sep2_x, p.y + 6.0f}, {p.x + sep2_x, p.y + height - 6.0f},
                    ImGui::ColorConvertFloat4ToU32(theme.separator_line));

        ImGui::SetCursorPos({sep2_x + 10.0f, text_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        // clip the topic text so it doesn't overlap the window buttons
        ImGui::PushClipRect({p.x + sep2_x + 10.0f, p.y},
                            {p.x + info_max_x, p.y + height}, true);
        ImGui::TextUnformatted(topic_.c_str());
        ImGui::PopClipRect();
        ImGui::PopStyleColor();
    }

    // drag-to-move is handled by SDL's hit test callback, not here.
    // doing it in both places causes the window to flicker because
    // SDL and our code fight over the window position every frame.

    // double-click title bar to maximize/restore
    if (window_) {
        ImVec2 mouse = ImGui::GetMousePos();
        bool in_titlebar = (mouse.y >= p.y && mouse.y < p.y + height &&
                            mouse.x >= p.x && mouse.x < p.x + btn_x);
        if (in_titlebar && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            wants_maximize_ = true;
        }
    }
}

} // namespace conduit::ui
