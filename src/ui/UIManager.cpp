#include "ui/UIManager.h"
#include <imgui.h>
#include <SDL.h>
#include <algorithm>

namespace conduit::ui {

UIManager::UIManager() {}

// invisible vertical splitter handle that you can drag to resize panes
// returns true while actively being dragged
bool UIManager::verticalSplitter(const char* id, float x, float y, float height,
                                  float* width_to_adjust, float min_w, float max_w,
                                  bool drag_left) {
    const float handle_w = 6.0f; // clickable area width
    const float visual_w = 1.0f; // the visible line

    ImVec2 screen_pos = ImGui::GetWindowPos();
    ImVec2 p0 = {screen_pos.x + x - handle_w * 0.5f, screen_pos.y + y};
    ImVec2 p1 = {screen_pos.x + x + handle_w * 0.5f, screen_pos.y + y + height};

    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton(id, {handle_w, height});

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    // change cursor to resize when hovering
    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    // draw the splitter line - brighter when hovered/dragged
    ImU32 color = hovered || active
        ? ImGui::ColorConvertFloat4ToU32({0.5f, 0.5f, 0.6f, 1.0f})
        : ImGui::ColorConvertFloat4ToU32({0.2f, 0.2f, 0.25f, 1.0f});

    ImGui::GetWindowDrawList()->AddRectFilled(
        {screen_pos.x + x - visual_w * 0.5f, screen_pos.y + y},
        {screen_pos.x + x + visual_w * 0.5f, screen_pos.y + y + height},
        color);

    // handle dragging
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float delta = ImGui::GetIO().MouseDelta.x;
        if (drag_left) delta = -delta; // for right-side panes, dragging left makes them wider
        *width_to_adjust += delta;
        *width_to_adjust = std::clamp(*width_to_adjust, min_w, max_w);
        return true;
    }

    return false;
}

void UIManager::render() {
    // fill the entire OS window - the OS handles the title bar, min/max/close
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

    // make sure the center pane doesn't get squeezed to nothing
    float min_center = 200.0f;
    float used = sidebar_left + sidebar_right + thread_w;
    if (used + min_center > win_w) {
        // scale everything down proportionally to fit
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

    // vertical layout — input bar height is dynamic based on content
    float title_h = layout_.title_bar_height;
    float status_h = layout_.status_bar_height;
    float max_input_h = win_h * 0.4f; // don't let input eat more than 40% of the window
    // we'll render the input bar later but need to know its height now
    // estimate based on content (the render call will use the actual height)
    float input_h = layout_.input_bar_height;
    float center_height = win_h - title_h - input_h - status_h;

    // ---- render panels ----

    // title bar (full width)
    title_bar_.render(0, 0, win_w, title_h, theme_);

    ImDrawList* wdl = ImGui::GetWindowDrawList();
    ImVec2 wpos = ImGui::GetWindowPos();

    // left sidebar
    if (layout_.show_buffer_list) {
        buffer_list_.render(0, title_h, sidebar_left, center_height, theme_);

        // hairline border so the sidebar doesn't just melt into the chat pane
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

        // matching hairline on the left edge of the nick list
        wdl->AddLine(
            {wpos.x + win_w - sidebar_right, wpos.y + title_h},
            {wpos.x + win_w - sidebar_right, wpos.y + title_h + center_height},
            ImGui::ColorConvertFloat4ToU32(theme_.separator_line));
    }

    // input bar — render with dynamic height, then place status bar after it
    float actual_input_h = input_bar_.render(0, title_h + center_height, win_w, max_input_h, theme_);
    // update for next frame's layout
    layout_.input_bar_height = actual_input_h;

    // status bar
    status_bar_.render(0, title_h + center_height + actual_input_h, win_w, status_h, theme_);

    // ---- draggable splitters between panes ----

    // splitter between buffer list and chat
    if (layout_.show_buffer_list) {
        verticalSplitter("##split_left", sidebar_left, title_h, center_height,
                         &layout_.buffer_list_width,
                         layout_.buffer_list_min, layout_.buffer_list_max);
    }

    // splitter between chat and thread panel (or nick list if no thread)
    if (thread_panel_.isOpen()) {
        verticalSplitter("##split_thread", sidebar_left + center_width, title_h, center_height,
                         &layout_.thread_panel_width,
                         layout_.thread_panel_min, layout_.thread_panel_max, true);
    }

    // splitter between thread/chat and nick list
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
            // can't paste right now because the popup stole focus from InputText.
            // set a flag and the input bar will handle it next frame.
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
