#include "ui/BufferList.h"
#include <imgui.h>

namespace conduit::ui {

BufferList::BufferList() {
    entries_ = {
        {"Conduit", false, false, 0, false, true, false, ""},
        {"#welcome", true, false, 0, false, false, false, ""},
    };
}

void BufferList::setEntries(const std::vector<BufferEntry>& entries) {
    std::string prev_id;
    if (selected_ >= 0 && selected_ < (int)entries_.size()) {
        prev_id = entries_[selected_].channel_id;
    }

    entries_ = entries;

    if (!prev_id.empty()) {
        for (int i = 0; i < (int)entries_.size(); i++) {
            if (entries_[i].channel_id == prev_id) {
                selected_ = i;
                return;
            }
        }
    }

    for (int i = 0; i < (int)entries_.size(); i++) {
        if (!entries_[i].is_separator && !entries_[i].is_section_header) {
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
    ImVec4 accent = theme.nick_colors[0];

    // track which section we're in for collapse logic
    std::string current_section;
    bool section_collapsed = false;

    for (int i = 0; i < (int)entries_.size(); i++) {
        const auto& entry = entries_[i];

        // org header separator
        if (entry.is_separator) {
            ImGui::Dummy({0, 2.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
            ImGui::SetCursorPosX(8.0f);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::PopStyleColor();

            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine({p.x + 6.0f, p.y}, {p.x + width - 6.0f, p.y},
                        ImGui::ColorConvertFloat4ToU32(theme.separator_line));
            ImGui::Dummy({0, 3.0f});
            continue;
        }

        // section header ("Channels", "Direct Messages")
        if (entry.is_section_header) {
            current_section = entry.name;
            section_collapsed = isSectionCollapsed(entry.name);

            ImGui::Dummy({0, 4.0f});
            ImVec2 sec_pos = ImGui::GetCursorScreenPos();

            // clickable section header
            ImGui::SetCursorPosX(6.0f);
            if (ImGui::InvisibleButton(("##sec_" + entry.name).c_str(),
                                       {width - 12.0f, line_height})) {
                toggleSection(entry.name);
                section_collapsed = isSectionCollapsed(entry.name);
            }

            // draw the label over the invisible button
            ImGui::SetCursorScreenPos(sec_pos);
            ImGui::SetCursorPosX(8.0f);

            // collapse/expand triangle
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            ImGui::TextUnformatted(section_collapsed ? "\xe2\x96\xb6" : "\xe2\x96\xbc"); // ▶ or ▼
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::PopStyleColor();

            // hover feedback
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::Dummy({0, 1.0f});
            continue;
        }

        // skip entries in collapsed sections
        if (section_collapsed) continue;

        bool is_selected = (i == selected_);
        ImVec2 cursor_before = ImGui::GetCursorScreenPos();

        // selection highlight
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

        if (ImGui::Selectable(("##buf" + std::to_string(i)).c_str(), is_selected,
                               ImGuiSelectableFlags_None, {width - 4.0f, line_height})) {
            selected_ = i;
        }

        // hover highlight
        if (ImGui::IsItemHovered() && !is_selected) {
            dl->AddRectFilled(
                cursor_before,
                {cursor_before.x + width, cursor_before.y + line_height},
                ImGui::ColorConvertFloat4ToU32({0.06f, 0.06f, 0.08f, 0.6f}));
        }

        // right-click context menu
        if (ImGui::IsItemClicked(1)) {
            right_clicked_channel_ = entry.channel_id;
            ImGui::OpenPopup(("##chan_ctx_" + std::to_string(i)).c_str());
        }

        // tooltip for truncated names
        {
            float right_margin = (entry.unread_count > 0) ? 40.0f : 8.0f;
            float avail_w = width - 10.0f - right_margin;
            float name_w = ImGui::CalcTextSize(entry.name.c_str()).x;
            if (ImGui::IsItemHovered() && name_w > avail_w && avail_w > 0) {
                ImGui::SetTooltip("%s", entry.name.c_str());
            }
        }

        // channel context menu
        if (ImGui::BeginPopup(("##chan_ctx_" + std::to_string(i)).c_str())) {
            if (ImGui::MenuItem("Mark as read")) {}
            if (ImGui::MenuItem("Leave channel")) {}
            ImGui::EndPopup();
        }

        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(10.0f);

        // unread indicator
        if (entry.has_unread) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.30f, 0.85f, 0.40f, 1.0f});
            ImGui::TextUnformatted("\xe2\x97\x8f");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{theme.text_dim.x, theme.text_dim.y,
                                                          theme.text_dim.z, 0.3f});
            ImGui::TextUnformatted("\xe2\x97\x8f");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);
        }

        // channel/DM name
        if (entry.has_unread || is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        }
        {
            float right_margin = (entry.unread_count > 0) ? 40.0f : 8.0f;
            float avail_w = width - ImGui::GetCursorPosX() - right_margin;
            float name_w = ImGui::CalcTextSize(entry.name.c_str()).x;
            if (name_w <= avail_w || avail_w <= 0) {
                ImGui::TextUnformatted(entry.name.c_str());
            } else {
                float dots_w = ImGui::CalcTextSize("..").x;
                float target_w = avail_w - dots_w;
                std::string truncated;
                for (size_t ci = 0; ci < entry.name.size(); ci++) {
                    truncated += entry.name[ci];
                    if (ImGui::CalcTextSize(truncated.c_str()).x >= target_w) break;
                }
                truncated += "..";
                ImGui::TextUnformatted(truncated.c_str());
            }
        }
        ImGui::PopStyleColor();

        // unread count badge
        if (entry.unread_count > 0) {
            std::string badge = std::to_string(entry.unread_count);
            float badge_w = ImGui::CalcTextSize(badge.c_str()).x + 8.0f;
            float badge_x = width - badge_w - 8.0f;
            float badge_y = cursor_before.y + (line_height - ImGui::GetTextLineHeight()) * 0.5f;

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
    while (next < (int)entries_.size() &&
           (entries_[next].is_separator || entries_[next].is_section_header)) next++;
    if (next < (int)entries_.size()) selected_ = next;
}

void BufferList::selectPrev() {
    int prev = selected_ - 1;
    while (prev >= 0 &&
           (entries_[prev].is_separator || entries_[prev].is_section_header)) prev--;
    if (prev >= 0) selected_ = prev;
}

} // namespace conduit::ui
