#include "ui/NickList.h"
#include <imgui.h>

namespace conduit::ui {

NickList::NickList() {}

void NickList::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_sidebar);
    ImGui::BeginChild("##nicklist", {width, height}, false);

    if (nicks_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(" (nobody home)");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    // tally up the headcount
    int online_count = 0;
    int away_count = 0;
    for (auto& n : nicks_) {
        if (n.is_online) online_count++;
        else away_count++;
    }

    // summary line at the top - wraps if needed
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    std::string summary = std::to_string(online_count) + " on, " + std::to_string(away_count) + " away";
    ImGui::SetCursorPosX(4.0f);
    ImGui::PushTextWrapPos(width - 4.0f);
    ImGui::TextUnformatted(summary.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // thin separator under the count
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sep = ImGui::GetCursorScreenPos();
    dl->AddLine({sep.x + 4.0f, sep.y + 1.0f}, {sep.x + width - 4.0f, sep.y + 1.0f},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));
    ImGui::Dummy({0, 4.0f});

    float line_height = ImGui::GetTextLineHeightWithSpacing();

    for (size_t ni = 0; ni < nicks_.size(); ni++) {
        const auto& nick = nicks_[ni];

        ImVec2 row_start = ImGui::GetCursorScreenPos();

        // invisible selectable for hover/click detection - the backbone of interactivity
        std::string sel_id = "##nick_" + std::to_string(ni);
        ImGui::Selectable(sel_id.c_str(), false, ImGuiSelectableFlags_None,
                          {width - 4.0f, line_height});

        bool item_hovered = ImGui::IsItemHovered();
        bool was_truncated = false;

        // hover highlight
        if (item_hovered) {
            dl->AddRectFilled(
                row_start,
                {row_start.x + width, row_start.y + line_height},
                ImGui::ColorConvertFloat4ToU32({0.06f, 0.06f, 0.08f, 0.6f}));
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // right-click context menu (don't set clicked_nick_ here, wait for menu)
        if (ImGui::IsItemClicked(1)) {
            ImGui::OpenPopup(("##nick_ctx_" + std::to_string(ni)).c_str());
        }

        // now draw the actual content on top of the selectable (same line)
        ImGui::SameLine(0, 0);
        ImGui::SetCursorScreenPos(row_start);

        // presence dot - filled circle for online, hollow for offline
        ImVec4 dot_color = nick.is_online
            ? ImVec4{0.30f, 0.85f, 0.40f, 1.0f}
            : ImVec4{0.40f, 0.40f, 0.45f, 1.0f};

        ImGui::SetCursorPosX(8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, dot_color);
        if (nick.is_online) {
            ImGui::TextUnformatted("\xe2\x97\x8f"); // filled
        } else {
            ImGui::TextUnformatted("\xe2\x97\x8b"); // hollow
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6.0f);

        // nick color based on their user id hash, dimmed if they're not around
        ImVec4 color = theme.nickColor(nick.name);
        if (!nick.is_online) {
            color.x *= 0.55f; color.y *= 0.55f; color.z *= 0.55f;
        }
        if (nick.is_bot) color = theme.text_dim;

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        std::string display = nick.is_bot ? nick.name + " [bot]" : nick.name;
        // truncate if too wide for the panel
        float avail = width - ImGui::GetCursorPosX() - 4.0f;
        float text_w = ImGui::CalcTextSize(display.c_str()).x;
        if (text_w > avail && avail > 20.0f) {
            float dots_w = ImGui::CalcTextSize("..").x;
            std::string trunc;
            for (size_t ci = 0; ci < display.size(); ci++) {
                trunc += display[ci];
                if (ImGui::CalcTextSize(trunc.c_str()).x >= avail - dots_w) break;
            }
            trunc += "..";
            ImGui::TextUnformatted(trunc.c_str());
            was_truncated = true;
        } else {
            ImGui::TextUnformatted(display.c_str());
        }
        ImGui::PopStyleColor();

        // tooltip for truncated names - don't leave people guessing
        if (item_hovered && was_truncated) {
            ImGui::SetTooltip("%s", display.c_str());
        }

        // nick context menu
        if (ImGui::BeginPopup(("##nick_ctx_" + std::to_string(ni)).c_str())) {
            if (ImGui::MenuItem("Open DM")) {
                clicked_nick_ = nick.name;
            }
            if (ImGui::MenuItem("Copy username")) {
                ImGui::SetClipboardText(nick.name.c_str());
            }
            ImGui::EndPopup();
        }

        // a little breathing room between entries
        ImGui::Dummy({0, 1.0f});
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
