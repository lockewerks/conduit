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
        ImGui::TextUnformatted(" (empty)");
        ImGui::PopStyleColor();
    }

    for (const auto& nick : nicks_) {
        const char* dot = nick.is_online ? "+" : " ";
        ImVec4 dot_color = nick.is_online
            ? ImVec4{0.3f, 0.8f, 0.3f, 1.0f}
            : ImVec4{0.4f, 0.4f, 0.4f, 1.0f};

        ImGui::PushStyleColor(ImGuiCol_Text, dot_color);
        ImGui::TextUnformatted(dot);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImVec4 color = theme.nickColor(nick.name);
        if (!nick.is_online) {
            color.x *= 0.6f; color.y *= 0.6f; color.z *= 0.6f;
        }
        if (nick.is_bot) color = theme.text_dim;

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        std::string display = nick.is_bot ? nick.name + " [bot]" : nick.name;
        ImGui::TextUnformatted(display.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
