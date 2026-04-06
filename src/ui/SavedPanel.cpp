#include "ui/SavedPanel.h"
#include <imgui.h>

namespace conduit::ui {

void SavedPanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##saved_panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::Text("Saved Items (%d)", (int)items_.size());
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (items_.empty()) {
        ImGui::TextDisabled("No saved items");
    }

    ImGui::BeginChild("##saved_scroll", ImVec2(0, 0), false);
    for (auto& item : items_) {
        ImGui::PushID(&item);

        // channel
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
        ImGui::TextUnformatted(("#" + item.channel_name).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // author
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        ImGui::TextUnformatted(item.author_name.c_str());
        ImGui::PopStyleColor();

        // snippet
        std::string snippet = item.text_snippet.substr(0, 100);
        if (item.text_snippet.size() > 100) snippet += "...";
        ImGui::TextWrapped("%s", snippet.c_str());

        // click to jump
        if (ImGui::IsItemClicked() && jump_cb_) {
            jump_cb_(item.channel_id, item.message_ts);
        }

        // unsave button
        if (ImGui::SmallButton("Unsave")) {
            if (unsave_cb_) unsave_cb_(item.channel_id, item.message_ts);
        }

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace conduit::ui
