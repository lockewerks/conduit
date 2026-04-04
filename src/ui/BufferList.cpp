#include "ui/BufferList.h"
#include <imgui.h>

namespace conduit::ui {

BufferList::BufferList() {
    // placeholder data so we can see the layout
    // this all gets replaced once we have real slack data
    entries_ = {
        {"LockeWerks", false, false, 0, false, true},
        {"#general", true, false, 0, false, false},
        {"#random", false, true, 3, false, false},
        {"#dev", false, false, 0, false, false},
        {"#design", false, true, 1, false, false},
        {"#incidents", false, false, 0, false, false},
        {"#food", false, true, 12, false, false},
        {"@alice", false, false, 0, true, false},
        {"@bob", false, true, 2, true, false},
        {"@carol", false, false, 0, true, false},
    };
}

void BufferList::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_sidebar);
    ImGui::BeginChild("##bufferlist", {width, height}, false);

    float line_height = ImGui::GetTextLineHeightWithSpacing();

    for (int i = 0; i < (int)entries_.size(); i++) {
        const auto& entry = entries_[i];

        if (entry.is_separator) {
            // org header - slightly brighter
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

        // unread indicator: bright white for unreads
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
    if (index >= 0 && index < (int)entries_.size()) {
        selected_ = index;
    }
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
