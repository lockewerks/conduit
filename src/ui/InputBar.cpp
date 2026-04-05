#include "ui/InputBar.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace conduit::ui {

float InputBar::render(float x, float y, float width, float max_height, const Theme& theme) {
    submitted_ = false;

    // calculate how tall the input needs to be based on text content
    float prompt_w = ImGui::CalcTextSize(("[" + channel_name_ + "] ").c_str()).x;
    float text_width = width - prompt_w - 12.0f;
    float line_h = ImGui::GetTextLineHeight();

    // measure how many lines the current text needs
    int line_count = 1;
    if (input_buf_[0] != '\0') {
        // rough estimate: count how many times the text wraps
        float text_w = ImGui::CalcTextSize(input_buf_).x;
        if (text_width > 0) {
            line_count = std::max(1, (int)(text_w / text_width) + 1);
        }
        // also count explicit newlines
        for (int i = 0; input_buf_[i]; i++) {
            if (input_buf_[i] == '\n') line_count++;
        }
    }

    float input_height = std::min(line_h * line_count + 8.0f, max_height);
    input_height = std::max(input_height, line_h + 6.0f); // minimum one line

    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(p, {p.x + width, p.y + input_height},
                       ImGui::ColorConvertFloat4ToU32(theme.bg_input));

    float text_y = y + 3.0f;

    // prompt: [#channel]
    ImGui::SetCursorPos({x + 4.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    std::string prompt = "[" + channel_name_ + "] ";
    ImGui::TextUnformatted(prompt.c_str());
    ImGui::PopStyleColor();

    float input_x = x + 4.0f + prompt_w;
    float input_w = width - input_x - 4.0f;

    ImGui::SetCursorPos({input_x, text_y});
    ImGui::PushItemWidth(input_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.0f, 2.0f});

    if (focus_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }

    // multiline input that grows with content
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CtrlEnterForNewLine |
                                 ImGuiInputTextFlags_EnterReturnsTrue;

    float multiline_h = input_height - 4.0f;
    if (ImGui::InputTextMultiline("##input", input_buf_, sizeof(input_buf_),
                                   {input_w, multiline_h}, flags)) {
        text_ = input_buf_;
        if (!text_.empty()) {
            submitted_ = true;
        }
        focus_input_ = true;
    }

    // input history (up arrow when empty)
    if (ImGui::IsItemFocused() && history_) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && input_buf_[0] == '\0') {
            std::string prev = history_->prev(channel_id_);
            if (!prev.empty()) {
                strncpy(input_buf_, prev.c_str(), sizeof(input_buf_) - 1);
                input_buf_[sizeof(input_buf_) - 1] = '\0';
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && input_buf_[0] == '\0') {
            std::string next = history_->next(channel_id_);
            strncpy(input_buf_, next.c_str(), sizeof(input_buf_) - 1);
            input_buf_[sizeof(input_buf_) - 1] = '\0';
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    return input_height;
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
