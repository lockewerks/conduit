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

    // summary line at the top
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    std::string summary = std::to_string(online_count) + " online";
    if (away_count > 0) summary += ", " + std::to_string(away_count) + " away";
    ImGui::SetCursorPosX(8.0f);
    ImGui::TextUnformatted(summary.c_str());
    ImGui::PopStyleColor();

    // thin separator under the count
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sep = ImGui::GetCursorScreenPos();
    dl->AddLine({sep.x + 4.0f, sep.y + 1.0f}, {sep.x + width - 4.0f, sep.y + 1.0f},
                ImGui::ColorConvertFloat4ToU32(theme.separator_line));
    ImGui::Dummy({0, 4.0f});

    for (const auto& nick : nicks_) {
        // presence dot - filled circle for online, hollow for offline
        ImVec4 dot_color = nick.is_online
            ? ImVec4{0.30f, 0.85f, 0.40f, 1.0f}
            : ImVec4{0.40f, 0.40f, 0.45f, 1.0f};

        ImGui::SetCursorPosX(8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, dot_color);
        // filled dot for online, hollow for offline
        if (nick.is_online) {
            ImGui::TextUnformatted("\xe2\x97\x8f"); // "●"
        } else {
            ImGui::TextUnformatted("\xe2\x97\x8b"); // "○"
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
        ImGui::TextUnformatted(display.c_str());
        ImGui::PopStyleColor();

        // a little breathing room between entries
        ImGui::Dummy({0, 1.0f});
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
