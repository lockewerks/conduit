#include "ui/OrgSwitcher.h"
#include <imgui.h>
#include <algorithm>

namespace conduit::ui {

void OrgSwitcher::open() {
    is_open_ = true;
    selected_ = 0;

    // pre-select the current active org so the user sees where they are
    for (int i = 0; i < (int)orgs_.size(); i++) {
        if (orgs_[i].is_active) {
            selected_ = i;
            break;
        }
    }
}

void OrgSwitcher::close() {
    is_open_ = false;
}

void OrgSwitcher::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_ || orgs_.empty()) return;

    // small centered popup - we're not building a dashboard here
    float popup_w = std::min(width * 0.4f, 350.0f);
    float line_h = ImGui::GetTextLineHeightWithSpacing();
    float popup_h = std::min(line_h * (float)(orgs_.size() + 2), height * 0.5f);
    float popup_x = x + (width - popup_w) * 0.5f;
    float popup_y = y + height * 0.2f;

    ImGui::SetCursorPos({popup_x, popup_y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // shadow + background, the usual song and dance
    dl->AddRectFilled({p.x - 2, p.y - 2}, {p.x + popup_w + 2, p.y + popup_h + 2},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.5f}), 4.0f);
    dl->AddRectFilled(p, {p.x + popup_w, p.y + popup_h},
                       ImGui::ColorConvertFloat4ToU32({0.12f, 0.12f, 0.16f, 1.0f}), 4.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0, 0, 0, 0});
    ImGui::BeginChild("##org_switcher", {popup_w, popup_h}, false);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted("Switch Organization");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // keyboard nav
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        selected_ = std::min(selected_ + 1, (int)orgs_.size() - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        selected_ = std::max(selected_ - 1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && selected_ < (int)orgs_.size()) {
        if (select_cb_) select_cb_(orgs_[selected_]);
        close();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }

    for (int i = 0; i < (int)orgs_.size(); i++) {
        auto& org = orgs_[i];
        bool is_sel = (i == selected_);

        if (is_sel) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(rp, {rp.x + popup_w - 8, rp.y + line_h},
                               ImGui::ColorConvertFloat4ToU32(theme.bg_selected));
        }

        // bullet for the active org
        if (org.is_active) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.unread_indicator);
            ImGui::TextUnformatted("*");
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, is_sel ? theme.text_bright : theme.text_default);
        ImGui::TextUnformatted(org.name.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
