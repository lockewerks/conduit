#include "ui/EmojiPicker.h"
#include "render/EmojiMap.h"
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

    // if nobody set the emoji list yet, populate from the unicode map
    // so we have something to show out of the box
    if (all_emojis_.empty()) {
        auto& emap = conduit::render::getEmojiMap();
        all_emojis_.reserve(emap.size());
        for (auto& [name, _] : emap) {
            all_emojis_.push_back(name);
        }
        std::sort(all_emojis_.begin(), all_emojis_.end());
    }

    filtered_ = all_emojis_;
}

void EmojiPicker::close() {
    is_open_ = false;
}

void EmojiPicker::render(const Theme& theme) {
    if (!is_open_) return;

    float popup_w = 360.0f;
    float popup_h = 320.0f;

    ImGui::SetCursorScreenPos({anchor_x_, anchor_y_ - popup_h});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // drop shadow because we're fancy like that
    dl->AddRectFilled({p.x - 1, p.y - 1}, {p.x + popup_w + 1, p.y + popup_h + 1},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.4f}), 4.0f);
    dl->AddRectFilled(p, {p.x + popup_w, p.y + popup_h},
                       ImGui::ColorConvertFloat4ToU32({0.13f, 0.13f, 0.17f, 1.0f}), 4.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0, 0, 0, 0});
    ImGui::BeginChild("##emoji_picker", {popup_w, popup_h}, false);

    // search box at the top - grab focus immediately so you can just start typing
    ImGui::PushItemWidth(popup_w - 16);
    ImGui::SetKeyboardFocusHere();
    bool changed = ImGui::InputText("##emoji_filter", filter_buf_, sizeof(filter_buf_));
    ImGui::PopItemWidth();

    if (changed) {
        filterEmojis();
        selected_ = 0;
    }

    // keyboard nav - arrow keys to move, enter to pick, escape to bail
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

    auto& emap = conduit::render::getEmojiMap();

    // render emojis in a grid - show unicode glyphs where we have them
    float cell_w = (popup_w - 16) / kGridCols;
    int col = 0;

    // cap display count so we don't choke on the full list
    int max_display = std::min((int)filtered_.size(), kGridCols * 24);

    for (int i = 0; i < max_display; i++) {
        bool is_sel = (i == selected_);

        if (is_sel) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(rp, {rp.x + cell_w, rp.y + ImGui::GetTextLineHeightWithSpacing()},
                               ImGui::ColorConvertFloat4ToU32(theme.bg_selected));
        }

        // try to show the actual unicode emoji glyph, fall back to :name: if not mapped
        auto emoji_it = emap.find(filtered_[i]);
        if (emoji_it != emap.end()) {
            // render the unicode glyph - looks way better than :text:
            ImGui::PushStyleColor(ImGuiCol_Text, is_sel ? theme.text_bright : ImVec4{1, 1, 1, 1});
            ImGui::TextUnformatted(emoji_it->second.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, is_sel ? theme.text_bright : theme.text_default);
            std::string display = filtered_[i];
            if (display.size() > 10) {
                display = display.substr(0, 9) + "~";
            }
            ImGui::Text(":%s:", display.c_str());
            ImGui::PopStyleColor();
        }

        // tooltip with the full name on hover (useful when the glyph is tiny)
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(":%s:", filtered_[i].c_str());
        }

        // click to select
        if (ImGui::IsItemClicked()) {
            if (select_cb_) select_cb_(filtered_[i]);
            close();
        }

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
