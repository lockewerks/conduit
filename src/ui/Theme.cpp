#include "ui/Theme.h"

namespace conduit::ui {

void Theme::apply() const {
    ImGuiStyle& style = ImGui::GetStyle();

    // make everything look flat and terminal-ish
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;

    style.WindowPadding = ImVec2(4.0f, 4.0f);
    style.FramePadding = ImVec2(4.0f, 2.0f);
    style.ItemSpacing = ImVec2(4.0f, 2.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 2.0f);
    style.ScrollbarSize = 10.0f;

    auto& c = style.Colors;

    c[ImGuiCol_WindowBg] = bg_main;
    c[ImGuiCol_ChildBg] = bg_main;
    c[ImGuiCol_PopupBg] = bg_sidebar;
    c[ImGuiCol_Border] = separator_line;
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};

    c[ImGuiCol_Text] = text_default;
    c[ImGuiCol_TextDisabled] = text_dim;

    c[ImGuiCol_FrameBg] = bg_input;
    c[ImGuiCol_FrameBgHovered] = {bg_input.x + 0.05f, bg_input.y + 0.05f, bg_input.z + 0.05f, 1.0f};
    c[ImGuiCol_FrameBgActive] = bg_selected;

    c[ImGuiCol_TitleBg] = bg_status;
    c[ImGuiCol_TitleBgActive] = bg_status;
    c[ImGuiCol_TitleBgCollapsed] = bg_status;
    c[ImGuiCol_MenuBarBg] = bg_status;

    c[ImGuiCol_ScrollbarBg] = bg_main;
    c[ImGuiCol_ScrollbarGrab] = separator_line;
    c[ImGuiCol_ScrollbarGrabHovered] = text_dim;
    c[ImGuiCol_ScrollbarGrabActive] = text_default;

    c[ImGuiCol_CheckMark] = text_bright;
    c[ImGuiCol_SliderGrab] = text_dim;
    c[ImGuiCol_SliderGrabActive] = text_default;

    c[ImGuiCol_Button] = bg_input;
    c[ImGuiCol_ButtonHovered] = bg_selected;
    c[ImGuiCol_ButtonActive] = {bg_selected.x + 0.1f, bg_selected.y + 0.1f, bg_selected.z + 0.1f, 1.0f};

    c[ImGuiCol_Header] = bg_selected;
    c[ImGuiCol_HeaderHovered] = {bg_selected.x + 0.05f, bg_selected.y + 0.05f, bg_selected.z + 0.05f, 1.0f};
    c[ImGuiCol_HeaderActive] = {bg_selected.x + 0.1f, bg_selected.y + 0.1f, bg_selected.z + 0.1f, 1.0f};

    c[ImGuiCol_Separator] = separator_line;
    c[ImGuiCol_SeparatorHovered] = text_dim;
    c[ImGuiCol_SeparatorActive] = text_default;

    c[ImGuiCol_ResizeGrip] = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered] = text_dim;
    c[ImGuiCol_ResizeGripActive] = text_default;

    c[ImGuiCol_Tab] = bg_sidebar;
    c[ImGuiCol_TabHovered] = bg_selected;
    // TabActive was renamed in newer imgui, using TabSelected for docking branch
    c[ImGuiCol_TabSelected] = bg_main;
    c[ImGuiCol_TabDimmed] = bg_sidebar;
    c[ImGuiCol_TabDimmedSelected] = bg_main;
}

} // namespace conduit::ui
