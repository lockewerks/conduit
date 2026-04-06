#include "ui/ChannelDialog.h"
#include <imgui.h>

namespace conduit::ui {

void ChannelDialog::open() {
    is_open_ = true;
    std::memset(name_buf_, 0, sizeof(name_buf_));
    std::memset(purpose_buf_, 0, sizeof(purpose_buf_));
    is_private_ = false;
    focus_name_ = true;
}

void ChannelDialog::render(const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetNextWindowSize(ImVec2(400, 220), ImGuiCond_FirstUseEver);
    ImGui::OpenPopup("Create Channel");

    if (ImGui::BeginPopupModal("Create Channel", &is_open_, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (focus_name_) {
            ImGui::SetKeyboardFocusHere();
            focus_name_ = false;
        }
        ImGui::Text("Channel name:");
        ImGui::InputText("##name", name_buf_, sizeof(name_buf_));

        ImGui::Text("Purpose (optional):");
        ImGui::InputText("##purpose", purpose_buf_, sizeof(purpose_buf_));

        ImGui::Checkbox("Private channel", &is_private_);

        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string name(name_buf_);
            if (!name.empty() && create_cb_) {
                create_cb_(name, is_private_);
                is_open_ = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            is_open_ = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace conduit::ui
