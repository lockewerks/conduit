#include "ui/InputBar.h"
#include <imgui.h>
#include <cstring>

namespace conduit::ui {

void InputBar::render(float x, float y, float width, float height, const Theme& theme) {
    submitted_ = false;

    ImGui::SetCursorPos({x, y});

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // background fill for the input area
    dl->AddRectFilled(p, {p.x + width, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.bg_input));

    // subtle top border to visually separate from the chat area above
    dl->AddLine(p, {p.x + width, p.y},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // ">" prompt character, like a proper terminal
    ImGui::SetCursorPos({x + 8.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.nick_colors[0]); // cyan, because style
    ImGui::TextUnformatted(">");
    ImGui::PopStyleColor();

    // channel indicator
    std::string label = "[" + channel_name_ + "]";
    ImGui::SetCursorPos({x + 22.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted(label.c_str());
    ImGui::PopStyleColor();

    float label_width = ImGui::CalcTextSize(label.c_str()).x;
    float input_x = x + 22.0f + label_width + 8.0f;
    float input_width = width - input_x - 8.0f;

    ImGui::SetCursorPos({input_x, y + 2.0f});
    ImGui::PushItemWidth(input_width);

    // the input field itself: subtle frame border when focused, invisible otherwise
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0}); // transparent bg, the parent bg shows through
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 4.0f});

    if (focus_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }

    if (ImGui::InputText("##input", input_buf_, sizeof(input_buf_),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        text_ = input_buf_;
        if (!text_.empty()) {
            submitted_ = true;
            // Application::handleInputSubmit will call clear()
        }
        focus_input_ = true;
    }

    // focus indication: thin underline when the input is active
    if (ImGui::IsItemActive()) {
        ImVec2 input_pos = ImGui::GetItemRectMin();
        ImVec2 input_end = ImGui::GetItemRectMax();
        dl->AddLine({input_pos.x, input_end.y + 1.0f}, {input_end.x, input_end.y + 1.0f},
                    ImGui::ColorConvertFloat4ToU32(theme.nick_colors[0]), 1.0f);
    }

    // input history: up arrow recalls past messages, because who wants to retype things
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

} // namespace conduit::ui
