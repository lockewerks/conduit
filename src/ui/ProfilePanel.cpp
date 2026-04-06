#include "ui/ProfilePanel.h"
#include <imgui.h>
#include <ctime>

namespace conduit::ui {

void ProfilePanel::open(const slack::User& user) {
    user_ = user;
    is_open_ = true;
}

void ProfilePanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##profile_panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextWrapped("%s", user_.effectiveName().c_str());
    ImGui::PopStyleColor();

    if (!user_.real_name.empty() && user_.real_name != user_.display_name) {
        ImGui::TextDisabled("%s", user_.real_name.c_str());
    }

    if (!user_.pronouns.empty()) {
        ImGui::TextDisabled("(%s)", user_.pronouns.c_str());
    }

    ImGui::Separator();

    // avatar
    if (image_renderer_ && !user_.avatar_url_192.empty()) {
        auto tex = image_renderer_->getTexture(user_.avatar_url_192);
        if (tex.texture_id != 0) {
            float size = std::min(width - 20.0f, 96.0f);
            float scale = size / std::max(static_cast<float>(tex.width), 1.0f);
            ImGui::Image((ImTextureID)(intptr_t)tex.texture_id,
                         ImVec2(tex.width * scale, tex.height * scale));
        } else {
            image_renderer_->renderPlaceholder("avatar", 96, 96);
        }
        ImGui::Spacing();
    }

    // status
    if (!user_.status_text.empty() || !user_.status_emoji.empty()) {
        ImGui::Text("%s %s", user_.status_emoji.c_str(), user_.status_text.c_str());
        ImGui::Spacing();
    }

    // presence
    if (user_.is_online) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        ImGui::TextUnformatted("Active");
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("Away");
    }

    ImGui::Separator();

    // details
    if (!user_.title.empty()) {
        ImGui::TextDisabled("Title");
        ImGui::TextWrapped("%s", user_.title.c_str());
        ImGui::Spacing();
    }

    if (!user_.email.empty()) {
        ImGui::TextDisabled("Email");
        ImGui::TextWrapped("%s", user_.email.c_str());
        ImGui::Spacing();
    }

    if (!user_.phone.empty()) {
        ImGui::TextDisabled("Phone");
        ImGui::TextWrapped("%s", user_.phone.c_str());
        ImGui::Spacing();
    }

    if (!user_.tz_label.empty()) {
        // show timezone + local time
        ImGui::TextDisabled("Timezone");
        time_t now = time(nullptr) + user_.tz_offset;
        struct tm t;
#ifdef _WIN32
        gmtime_s(&t, &now);
#else
        gmtime_r(&now, &t);
#endif
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M", &t);
        ImGui::Text("%s (%s local time)", user_.tz_label.c_str(), buf);
        ImGui::Spacing();
    }

    // badges
    if (user_.is_admin || user_.is_owner) {
        ImGui::Spacing();
        if (user_.is_owner) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.3f, 1.0f));
            ImGui::TextUnformatted("Workspace Owner");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
            ImGui::TextUnformatted("Workspace Admin");
            ImGui::PopStyleColor();
        }
    }

    if (user_.is_bot) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextUnformatted("Bot");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();

    // action buttons
    if (ImGui::Button("Message", ImVec2(width - 20, 0))) {
        if (open_dm_cb_) open_dm_cb_(user_.id);
    }

    if (ImGui::Button("Close", ImVec2(width - 20, 0))) {
        is_open_ = false;
    }

    ImGui::End();
}

} // namespace conduit::ui
