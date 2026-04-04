#include "ui/NickList.h"
#include <imgui.h>

namespace conduit::ui {

NickList::NickList() {
    // placeholder nicks - will be replaced with real channel members
    nicks_ = {
        {"alice", true, false},
        {"bob", true, false},
        {"carol", false, false},
        {"dave", true, false},
        {"eve", false, false},
        {"frank", true, false},
        {"bot", true, true},
    };
}

void NickList::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_sidebar);
    ImGui::BeginChild("##nicklist", {width, height}, false);

    for (const auto& nick : nicks_) {
        // presence dot: green = online, grey = offline
        const char* dot = nick.is_online ? "+" : " ";
        ImVec4 dot_color = nick.is_online
            ? ImVec4{0.3f, 0.8f, 0.3f, 1.0f}   // green-ish
            : ImVec4{0.4f, 0.4f, 0.4f, 1.0f};   // meh grey

        ImGui::PushStyleColor(ImGuiCol_Text, dot_color);
        ImGui::TextUnformatted(dot);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // nick in its color
        ImVec4 color = theme.nickColor(nick.name);
        if (!nick.is_online) {
            // dim it if offline, nobody cares about offline people
            // (kidding. sort of.)
            color.x *= 0.6f;
            color.y *= 0.6f;
            color.z *= 0.6f;
        }
        if (nick.is_bot) {
            color = theme.text_dim;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        std::string display = nick.is_bot ? nick.name + " [bot]" : nick.name;
        ImGui::TextUnformatted(display.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
