#include "ui/UIManager.h"
#include <imgui.h>

namespace conduit::ui {

UIManager::UIManager() {
    // don't apply theme here - imgui context doesn't exist yet
    // theme gets applied in Application::init() after ImGui::CreateContext()
}

void UIManager::render() {
    // we take over the entire window. no title bars, no docking chrome, just us.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("##conduit_main", nullptr, flags);
    ImGui::PopStyleVar();

    float win_w = ImGui::GetContentRegionAvail().x;
    float win_h = ImGui::GetContentRegionAvail().y;

    // figure out horizontal layout
    float sidebar_left = layout_.show_buffer_list ? layout_.buffer_list_width : 0.0f;
    float sidebar_right = layout_.show_nick_list ? layout_.nick_list_width : 0.0f;
    float center_width = win_w - sidebar_left - sidebar_right;

    // vertical layout
    float title_h = layout_.title_bar_height;
    float status_h = layout_.status_bar_height;
    float input_h = layout_.input_bar_height;
    float center_height = win_h - title_h - input_h - status_h;

    // title bar (full width at top)
    title_bar_.render(0, 0, win_w, title_h, theme_);

    // left sidebar (buffer list)
    if (layout_.show_buffer_list) {
        buffer_list_.render(0, title_h, sidebar_left, center_height, theme_);
    }

    // main chat area
    buffer_view_.render(sidebar_left, title_h, center_width, center_height, theme_);

    // right sidebar (nick list)
    if (layout_.show_nick_list) {
        nick_list_.render(sidebar_left + center_width, title_h, sidebar_right, center_height, theme_);
    }

    // input bar
    input_bar_.render(0, title_h + center_height, win_w, input_h, theme_);

    // status bar at the very bottom
    status_bar_.render(0, title_h + center_height + input_h, win_w, status_h, theme_);

    ImGui::End();
}

} // namespace conduit::ui
