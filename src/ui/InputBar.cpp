#include "ui/InputBar.h"
#include <imgui.h>
#include <cstring>

namespace conduit::ui {

void InputBar::render(float x, float y, float width, float height, const Theme& theme) {
    submitted_ = false;

    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // black background, same as everything else
    dl->AddRectFilled(p, {p.x + width, p.y + height},
                       ImGui::ColorConvertFloat4ToU32(theme.bg_input));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // IRC-style prompt: "[#channel] "
    ImGui::SetCursorPos({x + 4.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    std::string prompt = "[" + channel_name_ + "] ";
    ImGui::TextUnformatted(prompt.c_str());
    ImGui::PopStyleColor();

    float prompt_w = ImGui::CalcTextSize(prompt.c_str()).x;
    float input_x = x + 4.0f + prompt_w;
    float input_width = width - input_x - 4.0f;

    ImGui::SetCursorPos({input_x, y + 1.0f});
    ImGui::PushItemWidth(input_width);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.0f, 3.0f});

    if (focus_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }

    if (ImGui::InputText("##input", input_buf_, sizeof(input_buf_),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        text_ = input_buf_;
        if (!text_.empty()) {
            submitted_ = true;
        }
        focus_input_ = true;
    }

    // input history
    if (ImGui::IsItemFocused() && history_) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && input_buf_[0] == '\0') {
            std::string prev = history_->prev(channel_id_);
            if (!prev.empty()) {
                strncpy(input_buf_, prev.c_str(), sizeof(input_buf_) - 1);
                input_buf_[sizeof(input_buf_) - 1] = '\0';
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            std::string next = history_->next(channel_id_);
            strncpy(input_buf_, next.c_str(), sizeof(input_buf_) - 1);
            input_buf_[sizeof(input_buf_) - 1] = '\0';
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
}

void InputBar::pasteText(const std::string& text) {
    if (text.empty()) return;
    size_t current_len = strlen(input_buf_);
    size_t space = sizeof(input_buf_) - current_len - 1;
    if (space > 0) {
        strncat(input_buf_, text.c_str(), space);
        input_buf_[sizeof(input_buf_) - 1] = '\0';
    }
    focus_input_ = true;
}

} // namespace conduit::ui
