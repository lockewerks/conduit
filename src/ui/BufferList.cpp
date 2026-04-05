#include "ui/BufferList.h"
#include <imgui.h>

namespace conduit::ui {

BufferList::BufferList() {
    // start with a placeholder entry so the UI doesn't look totally empty
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

    float line_height = ImGui::GetTextLineHeightWithSpacing();

    for (int i = 0; i < (int)entries_.size(); i++) {
        const auto& entry = entries_[i];

        if (entry.is_separator) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
            ImGui::TextUnformatted((" " + entry.name).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            continue;
        }

        bool is_selected = (i == selected_);

        if (is_selected) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, {p.x + width, p.y + line_height},
                ImGui::ColorConvertFloat4ToU32(theme.bg_selected));
        }

        if (entry.has_unread) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        }

        std::string display = "  " + entry.name;
        if (entry.unread_count > 0) {
            display += " (" + std::to_string(entry.unread_count) + ")";
        }

        if (ImGui::Selectable(("##buf" + std::to_string(i)).c_str(), is_selected,
                               ImGuiSelectableFlags_None, {width - 8.0f, line_height})) {
            selected_ = i;
        }
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - width + 8.0f);
        ImGui::TextUnformatted(display.c_str());

        ImGui::PopStyleColor();
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
