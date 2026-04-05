#include "ui/UIManager.h"
#include <imgui.h>

namespace conduit::ui {

UIManager::UIManager() {
    // theme gets applied later by Application::init after ImGui context exists
}

void UIManager::render() {
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

    // horizontal layout
    float sidebar_left = layout_.show_buffer_list ? layout_.buffer_list_width : 0.0f;
    float sidebar_right = layout_.show_nick_list ? layout_.nick_list_width : 0.0f;

    // if thread panel is open, give it some space from the right
    float thread_width = thread_panel_.isOpen() ? 350.0f : 0.0f;

    float center_width = win_w - sidebar_left - sidebar_right - thread_width;

    // vertical layout
    float title_h = layout_.title_bar_height;
    float status_h = layout_.status_bar_height;
    float input_h = layout_.input_bar_height;
    float center_height = win_h - title_h - input_h - status_h;

    // title bar
    title_bar_.render(0, 0, win_w, title_h, theme_);

    // left sidebar
    if (layout_.show_buffer_list) {
        buffer_list_.render(0, title_h, sidebar_left, center_height, theme_);
    }

    // main chat area
    buffer_view_.render(sidebar_left, title_h, center_width, center_height, theme_);

    // thread panel (right of main chat, left of nick list)
    if (thread_panel_.isOpen()) {
        thread_panel_.render(sidebar_left + center_width, title_h,
                             thread_width, center_height, theme_);
    }

    // right sidebar (nick list)
    if (layout_.show_nick_list) {
        nick_list_.render(win_w - sidebar_right, title_h, sidebar_right, center_height, theme_);
    }

    // input bar
    input_bar_.render(0, title_h + center_height, win_w, input_h, theme_);

    // status bar
    status_bar_.render(0, title_h + center_height + input_h, win_w, status_h, theme_);

    // overlays (rendered on top of everything)
    if (search_panel_.isOpen()) {
        search_panel_.render(sidebar_left, title_h, center_width, center_height, theme_);
    }
    if (command_palette_.isOpen()) {
        command_palette_.render(0, 0, win_w, win_h, theme_);
    }

    ImGui::End();
}

} // namespace conduit::ui
