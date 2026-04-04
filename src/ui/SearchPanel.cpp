#include "ui/SearchPanel.h"
#include <imgui.h>
#include <cstring>

namespace conduit::ui {

void SearchPanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    submitted_ = false;

    // overlay panel from the top
    float panel_height = std::min(height * 0.6f, 400.0f);

    ImGui::SetCursorPos({x, y});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // semi-transparent overlay background
    dl->AddRectFilled(p, {p.x + width, p.y + panel_height},
                       ImGui::ColorConvertFloat4ToU32({0.1f, 0.1f, 0.15f, 0.95f}));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0, 0, 0, 0});
    ImGui::BeginChild("##search_panel", {width, panel_height}, false);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted("Search Messages");
    ImGui::PopStyleColor();

    ImGui::PushItemWidth(width - 32);
    if (ImGui::InputText("##search_input", search_buf_, sizeof(search_buf_),
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
        query_ = search_buf_;
        submitted_ = true;
    }
    ImGui::PopItemWidth();

    ImGui::Separator();

    // render search results
    for (auto& msg : results_) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(msg.user.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 40);
        ImGui::TextUnformatted(msg.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (results_.empty() && !query_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("no results. try different words.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

std::string SearchPanel::pendingQuery() {
    if (submitted_ && !query_.empty()) {
        std::string q = query_;
        submitted_ = false;
        return q;
    }
    return "";
}

} // namespace conduit::ui
