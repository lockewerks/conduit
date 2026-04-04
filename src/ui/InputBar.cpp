#include "ui/InputBar.h"
#include <imgui.h>
#include <cstring>

namespace conduit::ui {

void InputBar::render(float x, float y, float width, float height, const Theme& theme) {
    submitted_ = false;

    ImGui::SetCursorPos({x, y});

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, {p.x + width, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.bg_input));

    // channel indicator
    ImGui::SetCursorPos({x + 8.0f, y + 4.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted("[#general]");
    ImGui::PopStyleColor();

    float label_width = ImGui::CalcTextSize("[#general]").x;
    float input_x = x + 8.0f + label_width + 8.0f;
    float input_width = width - input_x - 8.0f;

    ImGui::SetCursorPos({input_x, y + 2.0f});
    ImGui::PushItemWidth(input_width);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.bg_input);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    // auto-focus the input because keyboard-first means never having to click
    if (focus_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

    if (ImGui::InputText("##input", input_buf_, sizeof(input_buf_), flags)) {
        text_ = input_buf_;
        if (!text_.empty()) {
            submitted_ = true;
            std::memset(input_buf_, 0, sizeof(input_buf_));
        }
        focus_input_ = true; // re-focus after submit
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
}

} // namespace conduit::ui
