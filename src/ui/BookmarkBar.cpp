#include "ui/BookmarkBar.h"
#include <imgui.h>

namespace conduit::ui {

float BookmarkBar::render(float x, float y, float width, const Theme& theme) {
    if (bookmarks_.empty()) return 0.0f;

    float height = 24.0f;
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##bookmark_bar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoScrollbar);

    for (auto& bm : bookmarks_) {
        std::string label = bm.emoji.empty() ? bm.title : bm.emoji + " " + bm.title;
        if (ImGui::SmallButton(label.c_str())) {
            if (open_url_cb_ && !bm.link.empty()) {
                open_url_cb_(bm.link);
            }
        }

        // right-click to remove
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            if (remove_cb_) remove_cb_(bm.id);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", bm.link.c_str());
        }

        ImGui::SameLine(0, 4);
    }

    ImGui::End();
    return height;
}

} // namespace conduit::ui
