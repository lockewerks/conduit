#include "ui/ActivityPanel.h"
#include <imgui.h>

namespace conduit::ui {

void ActivityPanel::addItem(const ActivityItem& item) {
    items_.insert(items_.begin(), item);
    if (items_.size() > MAX_ITEMS) items_.resize(MAX_ITEMS);
}

void ActivityPanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##activity_panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted("Activity");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (items_.empty()) {
        ImGui::TextDisabled("No recent activity");
    }

    ImGui::BeginChild("##activity_scroll", ImVec2(0, 0), false);
    for (auto& item : items_) {
        ImGui::PushID(&item);

        // channel name
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
        ImGui::TextUnformatted(("#" + item.channel_name).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        if (item.kind == ActivityItem::Kind::Mention) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::Text("%s mentioned you", item.actor_name.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.3f, 1.0f));
            ImGui::Text("%s reacted :%s:", item.actor_name.c_str(), item.emoji_name.c_str());
            ImGui::PopStyleColor();
        }

        // message snippet
        if (!item.message_text.empty()) {
            std::string snippet = item.message_text.substr(0, 80);
            if (item.message_text.size() > 80) snippet += "...";
            ImGui::TextDisabled("  %s", snippet.c_str());
        }

        // click to jump
        if (ImGui::IsItemClicked() && jump_cb_) {
            jump_cb_(item.channel_id, item.ts);
        }

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace conduit::ui
