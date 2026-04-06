#include "ui/UIManager.h"
#include <imgui.h>
#include <SDL.h>
#include <algorithm>

namespace conduit::ui {

UIManager::UIManager() {}

// direct mouse hit-test splitter — doesn't use InvisibleButton because
// child windows eat those clicks. instead we check mouse position directly
// against the splitter region and handle drag ourselves.
bool UIManager::verticalSplitter(const char* id, float x, float y, float height,
                                  float* width_to_adjust, float min_w, float max_w,
                                  bool drag_left) {
    const float handle_w = 10.0f;
    const float visual_w = 2.0f;
    // offset the hit zone away from the scrollbar side. for right-side
    // splitters (drag_left), the scrollbar is on the left, so shift right.
    // for the left splitter, the scrollbar is on the right, so shift left.
    float offset = drag_left ? 4.0f : -4.0f;

    ImVec2 wpos = ImGui::GetWindowPos();
    float sx = wpos.x + x;
    float sy = wpos.y + y;

    ImVec2 mouse = ImGui::GetIO().MousePos;
    float hit_center = sx + offset;
    bool mouse_in_rect = (mouse.x >= hit_center - handle_w * 0.5f &&
                          mouse.x <= hit_center + handle_w * 0.5f &&
                          mouse.y >= sy && mouse.y <= sy + height);

    // figure out which drag state bool to use based on ID
    bool* dragging = nullptr;
    if (std::string(id).find("left") != std::string::npos) dragging = &dragging_left_splitter_;
    else if (std::string(id).find("thread") != std::string::npos) dragging = &dragging_thread_splitter_;
    else dragging = &dragging_right_splitter_;

    // start dragging
    if (mouse_in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        *dragging = true;
    }

    // stop dragging
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        *dragging = false;
    }

    bool hovered = mouse_in_rect && !*dragging;
    bool active = *dragging;

    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    // draw splitter line on foreground so it's always visible
    ImU32 color = (hovered || active)
        ? ImGui::ColorConvertFloat4ToU32({0.5f, 0.5f, 0.6f, 1.0f})
        : ImGui::ColorConvertFloat4ToU32({0.2f, 0.2f, 0.25f, 1.0f});

    ImGui::GetForegroundDrawList()->AddRectFilled(
        {sx - visual_w * 0.5f, sy},
        {sx + visual_w * 0.5f, sy + height},
        color);

    if (active) {
        float delta = ImGui::GetIO().MouseDelta.x;
        if (drag_left) delta = -delta;
        *width_to_adjust += delta;
        *width_to_adjust = std::clamp(*width_to_adjust, min_w, max_w);
        return true;
    }

    return false;
}

void UIManager::render() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 2});
    ImGui::Begin("##conduit_main", nullptr, flags);
    ImGui::PopStyleVar();

    float win_w = ImGui::GetContentRegionAvail().x;
    float win_h = ImGui::GetContentRegionAvail().y;

    // proportionally scale pane widths when the window is resized
    if (last_win_w_ > 0.0f && win_w != last_win_w_) {
        float ratio = win_w / last_win_w_;
        layout_.buffer_list_width = std::clamp(
            layout_.buffer_list_width * ratio,
            layout_.buffer_list_min, layout_.buffer_list_max);
        layout_.nick_list_width = std::clamp(
            layout_.nick_list_width * ratio,
            layout_.nick_list_min, layout_.nick_list_max);
        if (thread_panel_.isOpen()) {
            layout_.thread_panel_width = std::clamp(
                layout_.thread_panel_width * ratio,
                layout_.thread_panel_min, layout_.thread_panel_max);
        }
    }
    last_win_w_ = win_w;

    // horizontal layout math
    float sidebar_left = layout_.show_buffer_list ? layout_.buffer_list_width : 0.0f;
    float sidebar_right = layout_.show_nick_list ? layout_.nick_list_width : 0.0f;
    float thread_w = thread_panel_.isOpen() ? layout_.thread_panel_width : 0.0f;

    float min_center = 200.0f;
    float used = sidebar_left + sidebar_right + thread_w;
    if (used + min_center > win_w) {
        float excess = used + min_center - win_w;
        float total = sidebar_left + sidebar_right + thread_w;
        if (total > 0) {
            if (sidebar_left > 0) sidebar_left -= excess * (sidebar_left / total);
            if (sidebar_right > 0) sidebar_right -= excess * (sidebar_right / total);
            if (thread_w > 0) thread_w -= excess * (thread_w / total);
            layout_.buffer_list_width = std::max(sidebar_left, layout_.buffer_list_min);
            layout_.nick_list_width = std::max(sidebar_right, layout_.nick_list_min);
            layout_.thread_panel_width = std::max(thread_w, layout_.thread_panel_min);
            sidebar_left = layout_.show_buffer_list ? layout_.buffer_list_width : 0.0f;
            sidebar_right = layout_.show_nick_list ? layout_.nick_list_width : 0.0f;
            thread_w = thread_panel_.isOpen() ? layout_.thread_panel_width : 0.0f;
        }
    }

    float center_width = win_w - sidebar_left - sidebar_right - thread_w;

    // vertical layout
    float title_h = layout_.title_bar_height;
    float status_h = layout_.status_bar_height;
    float max_input_h = win_h * 0.4f;
    float input_h = layout_.input_bar_height;
    float center_height = win_h - title_h - input_h - status_h;

    // ---- render panels ----

    title_bar_.render(0, 0, win_w, title_h, theme_);

    ImDrawList* wdl = ImGui::GetWindowDrawList();
    ImVec2 wpos = ImGui::GetWindowPos();

    // left sidebar
    if (layout_.show_buffer_list) {
        buffer_list_.render(0, title_h, sidebar_left, center_height, theme_);
        wdl->AddLine(
            {wpos.x + sidebar_left, wpos.y + title_h},
            {wpos.x + sidebar_left, wpos.y + title_h + center_height},
            ImGui::ColorConvertFloat4ToU32(theme_.separator_line));
    }

    // main chat area
    buffer_view_.render(sidebar_left, title_h, center_width, center_height, theme_);

    // thread panel
    if (thread_panel_.isOpen()) {
        thread_panel_.render(sidebar_left + center_width, title_h,
                             thread_w, center_height, theme_);
    }

    // right sidebar
    if (layout_.show_nick_list) {
        nick_list_.render(win_w - sidebar_right, title_h, sidebar_right, center_height, theme_);
        wdl->AddLine(
            {wpos.x + win_w - sidebar_right, wpos.y + title_h},
            {wpos.x + win_w - sidebar_right, wpos.y + title_h + center_height},
            ImGui::ColorConvertFloat4ToU32(theme_.separator_line));
    }

    // input bar
    float actual_input_h = input_bar_.render(0, title_h + center_height, win_w, max_input_h, theme_);
    layout_.input_bar_height = actual_input_h;

    // status bar
    status_bar_.render(0, title_h + center_height + actual_input_h, win_w, status_h, theme_);

    // ---- draggable splitters (direct mouse hit-test, no InvisibleButton) ----

    if (layout_.show_buffer_list) {
        verticalSplitter("##split_left", sidebar_left, title_h, center_height,
                         &layout_.buffer_list_width,
                         layout_.buffer_list_min, layout_.buffer_list_max);
    }

    if (thread_panel_.isOpen()) {
        verticalSplitter("##split_thread", sidebar_left + center_width, title_h, center_height,
                         &layout_.thread_panel_width,
                         layout_.thread_panel_min, layout_.thread_panel_max, true);
    }

    if (layout_.show_nick_list) {
        verticalSplitter("##split_right", win_w - sidebar_right, title_h, center_height,
                         &layout_.nick_list_width,
                         layout_.nick_list_min, layout_.nick_list_max, true);
    }

    // ---- overlays ----

    if (search_panel_.isOpen()) {
        search_panel_.render(sidebar_left, title_h, center_width, center_height, theme_);
    }
    if (command_palette_.isOpen()) {
        command_palette_.render(0, 0, win_w, win_h, theme_);
    }
    if (emoji_picker_.isOpen()) {
        emoji_picker_.render(theme_);
    }
    if (file_preview_.isOpen()) {
        file_preview_.render(win_w, win_h, theme_);
    }

    // ---- right-click context menu ----
    wants_paste_image_ = false;
    wants_paste_text_ = false;
    if (ImGui::BeginPopupContextWindow("##context_menu", ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            SDL_SetClipboardText(ImGui::GetClipboardText());
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            wants_paste_text_ = true;
        }
        if (ImGui::MenuItem("Paste Image")) {
            wants_paste_image_ = true;
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace conduit::ui
