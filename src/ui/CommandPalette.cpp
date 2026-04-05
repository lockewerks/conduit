#include "ui/CommandPalette.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace conduit::ui {

void CommandPalette::open() {
    is_open_ = true;
    std::memset(filter_buf_, 0, sizeof(filter_buf_));
    selected_ = 0;
    filtered_ = all_entries_;
}

void CommandPalette::close() {
    is_open_ = false;
}

void CommandPalette::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    float palette_w = std::min(width * 0.6f, 500.0f);
    float palette_h = std::min(height * 0.5f, 350.0f);
    float palette_x = x + (width - palette_w) * 0.5f;
    float palette_y = y + height * 0.15f;

    ImGui::SetCursorPos({palette_x, palette_y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // background with slight shadow feel
    dl->AddRectFilled({p.x - 2, p.y - 2}, {p.x + palette_w + 2, p.y + palette_h + 2},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.5f}), 4.0f);
    dl->AddRectFilled(p, {p.x + palette_w, p.y + palette_h},
                       ImGui::ColorConvertFloat4ToU32({0.12f, 0.12f, 0.16f, 1.0f}), 4.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0, 0, 0, 0});
    ImGui::BeginChild("##cmd_palette", {palette_w, palette_h}, false);

    // search input
    ImGui::PushItemWidth(palette_w - 16);
    ImGui::SetKeyboardFocusHere();
    bool changed = ImGui::InputText("##palette_filter", filter_buf_, sizeof(filter_buf_));
    ImGui::PopItemWidth();

    if (changed) {
        filterEntries();
        selected_ = 0;
    }

    // handle up/down/enter in the palette
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        selected_ = std::min(selected_ + 1, (int)filtered_.size() - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        selected_ = std::max(selected_ - 1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !filtered_.empty()) {
        if (select_cb_) select_cb_(filtered_[selected_]);
        close();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }

    ImGui::Separator();

    // render filtered results
    for (int i = 0; i < (int)filtered_.size() && i < 15; i++) {
        auto& entry = filtered_[i];
        bool is_selected = (i == selected_);

        if (is_selected) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(rp, {rp.x + palette_w - 8, rp.y + ImGui::GetTextLineHeightWithSpacing()},
                               ImGui::ColorConvertFloat4ToU32(theme.bg_selected));
        }

        ImGui::PushStyleColor(ImGuiCol_Text, is_selected ? theme.text_bright : theme.text_default);
        ImGui::TextUnformatted(entry.display.c_str());
        ImGui::PopStyleColor();

        // right-aligned category hint so you know what you're looking at
        ImGui::SameLine(palette_w - ImGui::CalcTextSize(entry.category.c_str()).x - 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, {theme.text_dim.x, theme.text_dim.y, theme.text_dim.z, 0.5f});
        ImGui::TextUnformatted(entry.category.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CommandPalette::filterEntries() {
    std::string filter(filter_buf_);
    if (filter.empty()) {
        filtered_ = all_entries_;
        return;
    }

    filtered_.clear();
    for (auto& entry : all_entries_) {
        if (fuzzyMatch(entry.display, filter)) {
            filtered_.push_back(entry);
        }
    }
}

bool CommandPalette::fuzzyMatch(const std::string& text, const std::string& filter) const {
    // simple subsequence match - good enough for a command palette
    size_t fi = 0;
    for (size_t ti = 0; ti < text.size() && fi < filter.size(); ti++) {
        if (std::tolower(text[ti]) == std::tolower(filter[fi])) {
            fi++;
        }
    }
    return fi == filter.size();
}

} // namespace conduit::ui
