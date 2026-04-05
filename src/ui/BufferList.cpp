#include "ui/BufferList.h"
#include <imgui.h>

namespace conduit::ui {

BufferList::BufferList() {
    // placeholder so it's not staring into the void on first launch
    entries_ = {
        {"Conduit", false, false, 0, false, true, ""},
        {"#welcome", true, false, 0, false, false, ""},
    };
}

void BufferList::setEntries(const std::vector<BufferEntry>& entries) {
    // preserve selection if possible
    std::string prev_id;
    if (selected_ >= 0 && selected_ < (int)entries_.size()) {
        prev_id = entries_[selected_].channel_id;
    }

    entries_ = entries;

    // try to re-select the same channel
    if (!prev_id.empty()) {
        for (int i = 0; i < (int)entries_.size(); i++) {
            if (entries_[i].channel_id == prev_id) {
                selected_ = i;
                return;
            }
        }
    }

    // couldn't find it, default to first non-separator
    for (int i = 0; i < (int)entries_.size(); i++) {
        if (!entries_[i].is_separator) {
            selected_ = i;
            return;
        }
    }
}

void BufferList::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_sidebar);
    ImGui::BeginChild("##bufferlist", {width, height}, false);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float line_height = ImGui::GetTextLineHeightWithSpacing();

    // thin accent color for the selection indicator
    ImVec4 accent = theme.nick_colors[0]; // that nice cyan

    for (int i = 0; i < (int)entries_.size(); i++) {
        const auto& entry = entries_[i];

        if (entry.is_separator) {
            // org header - bold-ish uppercase vibes
            ImGui::Dummy({0, 2.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
            ImGui::SetCursorPosX(8.0f);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::PopStyleColor();

            // subtle separator line underneath
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine({p.x + 6.0f, p.y}, {p.x + width - 6.0f, p.y},
                        ImGui::ColorConvertFloat4ToU32(theme.separator_line));
            ImGui::Dummy({0, 3.0f});
            continue;
        }

        bool is_selected = (i == selected_);
        ImVec2 cursor_before = ImGui::GetCursorScreenPos();

        // selected item: thin left accent bar instead of garish full-row highlight
        if (is_selected) {
            dl->AddRectFilled(
                cursor_before,
                {cursor_before.x + width, cursor_before.y + line_height},
                ImGui::ColorConvertFloat4ToU32({theme.bg_selected.x, theme.bg_selected.y,
                                                 theme.bg_selected.z, 0.5f}));
            dl->AddRectFilled(
                cursor_before,
                {cursor_before.x + 3.0f, cursor_before.y + line_height},
                ImGui::ColorConvertFloat4ToU32(accent));
        }

        // invisible selectable for click handling (covers the full row)
        if (ImGui::Selectable(("##buf" + std::to_string(i)).c_str(), is_selected,
                               ImGuiSelectableFlags_None, {width - 4.0f, line_height})) {
            selected_ = i;
        }
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(10.0f); // indented under the org header

        // unread indicator: green bullet for channels with new stuff
        if (entry.has_unread) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.30f, 0.85f, 0.40f, 1.0f});
            ImGui::TextUnformatted("\xe2\x97\x8f"); // "●"
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);
        } else {
            // faint dot so alignment stays consistent
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{theme.text_dim.x, theme.text_dim.y,
                                                          theme.text_dim.z, 0.3f});
            ImGui::TextUnformatted("\xe2\x97\x8f"); // "●"
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);
        }

        // channel name - bright if unread, dim if old news
        if (entry.has_unread || is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        }
        ImGui::TextUnformatted(entry.name.c_str());
        ImGui::PopStyleColor();

        // unread count badge, tucked to the right
        if (entry.unread_count > 0) {
            std::string badge = std::to_string(entry.unread_count);
            float badge_w = ImGui::CalcTextSize(badge.c_str()).x + 8.0f;
            float badge_x = width - badge_w - 8.0f;
            float badge_y = cursor_before.y + (line_height - ImGui::GetTextLineHeight()) * 0.5f;

            // little pill-shaped badge
            dl->AddRectFilled(
                {cursor_before.x + badge_x, badge_y - 1.0f},
                {cursor_before.x + badge_x + badge_w, badge_y + ImGui::GetTextLineHeight() + 1.0f},
                ImGui::ColorConvertFloat4ToU32(theme.mention_badge),
                6.0f);

            ImGui::SameLine(0, 0);
            ImGui::SetCursorPosX(badge_x + 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
            ImGui::TextUnformatted(badge.c_str());
            ImGui::PopStyleColor();
        }

        // tiny gap between entries so they don't blur together
        ImGui::Dummy({0, 1.0f});
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void BufferList::select(int index) {
    if (index >= 0 && index < (int)entries_.size()) selected_ = index;
}

void BufferList::selectNext() {
    int next = selected_ + 1;
    while (next < (int)entries_.size() && entries_[next].is_separator) next++;
    if (next < (int)entries_.size()) selected_ = next;
}

void BufferList::selectPrev() {
    int prev = selected_ - 1;
    while (prev >= 0 && entries_[prev].is_separator) prev--;
    if (prev >= 0) selected_ = prev;
}

} // namespace conduit::ui
