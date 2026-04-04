#include "ui/EmojiPicker.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace conduit::ui {

void EmojiPicker::open(float anchor_x, float anchor_y) {
    is_open_ = true;
    anchor_x_ = anchor_x;
    anchor_y_ = anchor_y;
    std::memset(filter_buf_, 0, sizeof(filter_buf_));
    selected_ = 0;
    filtered_ = all_emojis_;
}

void EmojiPicker::close() {
    is_open_ = false;
}

void EmojiPicker::render(const Theme& theme) {
    if (!is_open_) return;

    float popup_w = 320.0f;
    float popup_h = 280.0f;

    ImGui::SetCursorScreenPos({anchor_x_, anchor_y_ - popup_h});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled({p.x - 1, p.y - 1}, {p.x + popup_w + 1, p.y + popup_h + 1},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.4f}), 4.0f);
    dl->AddRectFilled(p, {p.x + popup_w, p.y + popup_h},
                       ImGui::ColorConvertFloat4ToU32({0.13f, 0.13f, 0.17f, 1.0f}), 4.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0, 0, 0, 0});
    ImGui::BeginChild("##emoji_picker", {popup_w, popup_h}, false);

    // search box at the top
    ImGui::PushItemWidth(popup_w - 16);
    ImGui::SetKeyboardFocusHere();
    bool changed = ImGui::InputText("##emoji_filter", filter_buf_, sizeof(filter_buf_));
    ImGui::PopItemWidth();

    if (changed) {
        filterEmojis();
        selected_ = 0;
    }

    // keyboard nav - move through grid
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        selected_ = std::min(selected_ + 1, (int)filtered_.size() - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        selected_ = std::max(selected_ - 1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        selected_ = std::min(selected_ + kGridCols, (int)filtered_.size() - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        selected_ = std::max(selected_ - kGridCols, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !filtered_.empty()) {
        if (select_cb_) select_cb_(filtered_[selected_]);
        close();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }

    ImGui::Separator();

    // render emojis in a grid
    float cell_w = (popup_w - 16) / kGridCols;
    int col = 0;

    // cap at a sane number so we don't murder the frame rate
    int max_display = std::min((int)filtered_.size(), kGridCols * 20);

    for (int i = 0; i < max_display; i++) {
        bool is_sel = (i == selected_);

        if (is_sel) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(rp, {rp.x + cell_w, rp.y + ImGui::GetTextLineHeightWithSpacing()},
                               ImGui::ColorConvertFloat4ToU32(theme.bg_selected));
        }

        ImGui::PushStyleColor(ImGuiCol_Text, is_sel ? theme.text_bright : theme.text_default);

        // show a truncated name that fits in the cell
        // full names like :slightly_smiling_face: are absurdly long
        std::string display = filtered_[i];
        if (display.size() > 10) {
            display = display.substr(0, 9) + "~";
        }
        ImGui::Text(":%s:", display.c_str());
        ImGui::PopStyleColor();

        col++;
        if (col < kGridCols && i + 1 < max_display) {
            ImGui::SameLine(cell_w * col);
        } else {
            col = 0;
        }
    }

    if (filtered_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("nothing matches. slack has 3000+ emoji and none of them fit?");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EmojiPicker::filterEmojis() {
    std::string filter(filter_buf_);
    if (filter.empty()) {
        filtered_ = all_emojis_;
        return;
    }

    // lowercase the filter once
    std::string lower_filter = filter;
    std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);

    filtered_.clear();
    for (auto& name : all_emojis_) {
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        if (lower_name.find(lower_filter) != std::string::npos) {
            filtered_.push_back(name);
        }
    }
}

} // namespace conduit::ui
