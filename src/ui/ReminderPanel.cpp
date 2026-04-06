#include "ui/ReminderPanel.h"
#include <imgui.h>
#include <ctime>

namespace conduit::ui {

void ReminderPanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##reminder_panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted("Reminders");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (reminders_.empty()) {
        ImGui::TextDisabled("No reminders");
    }

    ImGui::BeginChild("##reminders_scroll", ImVec2(0, 0), false);
    for (auto& r : reminders_) {
        if (r.is_complete) continue;

        ImGui::PushID(r.id.c_str());

        // time
        time_t t = static_cast<time_t>(r.time);
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", &tm_buf);

        ImGui::TextDisabled("%s", time_str);
        ImGui::TextWrapped("%s", r.text.c_str());

        if (ImGui::SmallButton("Done")) {
            if (complete_cb_) complete_cb_(r.id);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) {
            if (delete_cb_) delete_cb_(r.id);
        }

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace conduit::ui
