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
    std::string label = "[" + channel_name_ + "]";
    ImGui::SetCursorPos({x + 8.0f, y + 4.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted(label.c_str());
    ImGui::PopStyleColor();

    float label_width = ImGui::CalcTextSize(label.c_str()).x;
    float input_x = x + 8.0f + label_width + 8.0f;
    float input_width = width - input_x - 8.0f;

    ImGui::SetCursorPos({input_x, y + 2.0f});
    ImGui::PushItemWidth(input_width);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.bg_input);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    if (focus_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }

    if (ImGui::InputText("##input", input_buf_, sizeof(input_buf_),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        text_ = input_buf_;
        if (!text_.empty()) {
            submitted_ = true;
            // don't clear input_buf_ here, Application::handleInputSubmit will call clear()
        }
        focus_input_ = true;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
}

} // namespace conduit::ui
