#include "ui/Theme.h"

namespace conduit::ui {

void Theme::apply() const {
    ImGuiStyle& style = ImGui::GetStyle();

    // zero rounding everywhere. terminals don't have rounded corners.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    // minimal borders - just enough to see panel edges
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // tight spacing for that packed terminal feel
    style.WindowPadding = ImVec2(4.0f, 4.0f);
    style.FramePadding = ImVec2(4.0f, 2.0f);
    style.ItemSpacing = ImVec2(4.0f, 2.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 2.0f);
    style.ScrollbarSize = 8.0f; // thin scrollbar, out of the way

    auto& c = style.Colors;

    c[ImGuiCol_WindowBg] = bg_main;
    c[ImGuiCol_ChildBg] = bg_main;
    c[ImGuiCol_PopupBg] = {0.05f, 0.05f, 0.05f, 0.95f};
    c[ImGuiCol_Border] = separator_line;
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};

    c[ImGuiCol_Text] = text_default;
    c[ImGuiCol_TextDisabled] = text_dim;

    c[ImGuiCol_FrameBg] = bg_input;
    c[ImGuiCol_FrameBgHovered] = {0.08f, 0.08f, 0.08f, 1.0f};
    c[ImGuiCol_FrameBgActive] = bg_selected;

    c[ImGuiCol_TitleBg] = bg_status;
    c[ImGuiCol_TitleBgActive] = bg_status;
    c[ImGuiCol_TitleBgCollapsed] = bg_status;
    c[ImGuiCol_MenuBarBg] = bg_status;

    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0}; // invisible scrollbar track
    c[ImGuiCol_ScrollbarGrab] = {0.20f, 0.20f, 0.20f, 0.5f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.30f, 0.30f, 0.30f, 0.7f};
    c[ImGuiCol_ScrollbarGrabActive] = {0.40f, 0.40f, 0.40f, 0.9f};

    c[ImGuiCol_CheckMark] = text_bright;
    c[ImGuiCol_SliderGrab] = text_dim;
    c[ImGuiCol_SliderGrabActive] = text_default;

    c[ImGuiCol_Button] = {0.08f, 0.08f, 0.08f, 1.0f};
    c[ImGuiCol_ButtonHovered] = bg_selected;
    c[ImGuiCol_ButtonActive] = {0.18f, 0.18f, 0.25f, 1.0f};

    c[ImGuiCol_Header] = bg_selected;
    c[ImGuiCol_HeaderHovered] = {0.15f, 0.15f, 0.20f, 1.0f};
    c[ImGuiCol_HeaderActive] = {0.18f, 0.18f, 0.25f, 1.0f};

    c[ImGuiCol_Separator] = separator_line;
    c[ImGuiCol_SeparatorHovered] = text_dim;
    c[ImGuiCol_SeparatorActive] = text_default;

    c[ImGuiCol_ResizeGrip] = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered] = text_dim;
    c[ImGuiCol_ResizeGripActive] = text_default;

    c[ImGuiCol_Tab] = bg_sidebar;
    c[ImGuiCol_TabHovered] = bg_selected;
    c[ImGuiCol_TabSelected] = bg_main;
    c[ImGuiCol_TabDimmed] = bg_sidebar;
    c[ImGuiCol_TabDimmedSelected] = bg_main;
}

} // namespace conduit::ui
