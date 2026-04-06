#include "ui/InputBar.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace conduit::ui {

float InputBar::render(float x, float y, float width, float max_height, const Theme& theme) {
    submitted_ = false;

    float line_h = ImGui::GetTextLineHeight();

    // input height calculation
    float prompt_w = ImGui::CalcTextSize(("[" + channel_name_ + "] ").c_str()).x;
    float text_width = width - prompt_w - 12.0f;

    int line_count = 1;
    if (input_buf_[0] != '\0') {
        float text_w = ImGui::CalcTextSize(input_buf_).x;
        if (text_width > 0) {
            line_count = std::max(1, (int)(text_w / text_width) + 1);
        }
        for (int i = 0; input_buf_[i]; i++) {
            if (input_buf_[i] == '\n') line_count++;
        }
    }

    float input_height = std::min(line_h * line_count + 8.0f, max_height);
    input_height = std::max(input_height, line_h + 6.0f);

    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(p, {p.x + width, p.y + input_height},
                       ImGui::ColorConvertFloat4ToU32(theme.bg_input));

    float text_y = y + 3.0f;

    ImGui::SetCursorPos({x + 4.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted(("[" + channel_name_ + "] ").c_str());
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

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CtrlEnterForNewLine |
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_CallbackAlways;

    // intercept tab/arrow keys when autocomplete is open
    bool tab_pressed = false;

    float multiline_h = input_height - 4.0f;
    if (ImGui::InputTextMultiline("##input", input_buf_, sizeof(input_buf_),
                                   {input_w, multiline_h}, flags,
                                   [](ImGuiInputTextCallbackData* data) -> int {
                                       return 0;
                                   })) {
        // Enter always submits the message — Tab is for autocomplete
        ac_open_ = false;
        text_ = input_buf_;
        if (!text_.empty()) {
            submitted_ = true;
        }
        focus_input_ = true;
    }

    // track if input is focused for autocomplete
    bool input_focused = ImGui::IsItemActive();

    // Tab completion — cycles through matches like IRC/WeeChat
    if (ImGui::IsItemFocused() && tab_complete_) {
        std::string current(input_buf_);
        // reset cycling when user types something new (not via tab)
        if (current != last_input_for_tab_ && !ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            tab_complete_->reset();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            int cursor = (int)current.size();
            auto result = tab_complete_->complete(current, cursor);
            if (result.has_candidates) {
                std::memset(input_buf_, 0, sizeof(input_buf_));
                std::memcpy(input_buf_, result.completed_text.data(),
                            std::min(result.completed_text.size(), sizeof(input_buf_) - 1));
                last_input_for_tab_ = std::string(input_buf_);
            }
        } else {
            last_input_for_tab_ = current;
        }
    }

    // input history
    if (ImGui::IsItemFocused() && history_ && !ac_open_) {
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

    // ghost text
    if (input_buf_[0] == '\0' && !ImGui::IsItemActive()) {
        ImVec2 input_pos = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(
            {input_pos.x + 4, input_pos.y + 2},
            ImGui::ColorConvertFloat4ToU32({theme.text_dim.x, theme.text_dim.y, theme.text_dim.z, 0.4f}),
            "Type a message or /command...");
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    // ---- autocomplete popup ----
    if (input_focused && ac_provider_) {
        updateAutocomplete();

        if (ac_open_ && !ac_matches_.empty()) {
            // keyboard nav for autocomplete (check before render so it's responsive)
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                ac_selected_ = std::max(0, ac_selected_ - 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                ac_selected_ = std::min((int)ac_matches_.size() - 1, ac_selected_ + 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                applyAutocomplete(ac_matches_[ac_selected_]);
                ac_open_ = false;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ac_open_ = false;
            }

            // render the popup above the input bar
            int max_show = std::min((int)ac_matches_.size(), 8);
            float popup_h = max_show * ImGui::GetTextLineHeightWithSpacing() + 4.0f;
            float popup_w = std::min(width * 0.5f, 300.0f);
            float popup_x = p.x + prompt_w;
            float popup_y = p.y - popup_h;

            ImGui::GetForegroundDrawList()->AddRectFilled(
                {popup_x, popup_y}, {popup_x + popup_w, popup_y + popup_h},
                ImGui::ColorConvertFloat4ToU32({0.10f, 0.10f, 0.14f, 0.97f}), 4.0f);
            ImGui::GetForegroundDrawList()->AddRect(
                {popup_x, popup_y}, {popup_x + popup_w, popup_y + popup_h},
                ImGui::ColorConvertFloat4ToU32({0.25f, 0.25f, 0.30f, 1.0f}), 4.0f);

            float item_y = popup_y + 2.0f;
            float item_h = ImGui::GetTextLineHeightWithSpacing();

            // scroll to keep selected visible
            int scroll_start = std::max(0, ac_selected_ - max_show + 1);

            for (int i = scroll_start; i < std::min(scroll_start + max_show, (int)ac_matches_.size()); i++) {
                bool is_sel = (i == ac_selected_);

                if (is_sel) {
                    ImGui::GetForegroundDrawList()->AddRectFilled(
                        {popup_x + 2, item_y}, {popup_x + popup_w - 2, item_y + item_h},
                        ImGui::ColorConvertFloat4ToU32({0.20f, 0.25f, 0.40f, 1.0f}), 2.0f);
                }

                // prefix indicator
                std::string display;
                if (ac_trigger_ == '/') display = "/" + ac_matches_[i];
                else if (ac_trigger_ == '@') display = "@" + ac_matches_[i];
                else if (ac_trigger_ == ':') display = ":" + ac_matches_[i] + ":";
                else if (ac_trigger_ == '#') display = "#" + ac_matches_[i];
                else display = ac_matches_[i];

                ImU32 text_color = is_sel
                    ? ImGui::ColorConvertFloat4ToU32(theme.text_bright)
                    : ImGui::ColorConvertFloat4ToU32(theme.text_default);
                ImGui::GetForegroundDrawList()->AddText(
                    {popup_x + 8, item_y + 1}, text_color, display.c_str());

                item_y += item_h;
            }
        }
    } else {
        ac_open_ = false;
    }

    return input_height;
}

void InputBar::updateAutocomplete() {
    if (!ac_provider_) return;

    std::string text(input_buf_);
    if (text.empty()) {
        ac_open_ = false;
        return;
    }

    // scan backwards from end of text to find trigger character
    int len = (int)text.size();
    int trigger_pos = -1;
    char trigger = 0;

    for (int i = len - 1; i >= 0; i--) {
        char c = text[i];
        if (c == ' ' || c == '\n') break; // stop at whitespace
        if (c == '@' || c == '/' || c == ':' || c == '#') {
            // / only valid at start of line
            if (c == '/' && i != 0) continue;
            trigger_pos = i;
            trigger = c;
            break;
        }
    }

    if (trigger_pos < 0) {
        ac_open_ = false;
        return;
    }

    std::string prefix = text.substr(trigger_pos + 1);

    // only query if something changed
    if (trigger == ac_trigger_ && prefix == ac_prefix_ && ac_open_) return;

    ac_trigger_ = trigger;
    ac_trigger_pos_ = trigger_pos;
    ac_prefix_ = prefix;
    ac_matches_ = ac_provider_(trigger, prefix);
    ac_selected_ = 0;
    ac_open_ = !ac_matches_.empty();
}

void InputBar::applyAutocomplete(const std::string& match) {
    if (ac_trigger_pos_ < 0) return;

    std::string text(input_buf_);
    std::string replacement;
    if (ac_trigger_ == '@') replacement = "@" + match + " ";
    else if (ac_trigger_ == '/') replacement = "/" + match + " ";
    else if (ac_trigger_ == ':') replacement = ":" + match + ": ";
    else if (ac_trigger_ == '#') replacement = "#" + match + " ";
    else replacement = match + " ";

    text = text.substr(0, ac_trigger_pos_) + replacement;
    std::memset(input_buf_, 0, sizeof(input_buf_));
    std::memcpy(input_buf_, text.data(), std::min(text.size(), sizeof(input_buf_) - 1));

    ac_open_ = false;
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
