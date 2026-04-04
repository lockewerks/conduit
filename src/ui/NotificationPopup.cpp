#include "ui/NotificationPopup.h"
#include <imgui.h>
#include <algorithm>

namespace conduit::ui {

void NotificationPopup::show(const std::string& title, const std::string& body) {
    title_ = title;
    body_ = body;
    visible_ = true;
    show_time_ = std::chrono::steady_clock::now();
}

void NotificationPopup::render(float screen_w, float screen_h, const Theme& theme) {
    if (!visible_) return;

    float alpha = currentAlpha();
    if (alpha <= 0.0f) {
        visible_ = false;
        return;
    }

    // shove it in the top-right corner where notifications belong
    float toast_w = 300.0f;
    float toast_h = 60.0f;
    float margin = 12.0f;
    float toast_x = screen_w - toast_w - margin;
    float toast_y = margin;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // background with alpha
    ImVec4 bg = {0.15f, 0.15f, 0.20f, 0.92f * alpha};
    ImVec4 border = {0.30f, 0.30f, 0.40f, 0.6f * alpha};
    dl->AddRectFilled({toast_x, toast_y}, {toast_x + toast_w, toast_y + toast_h},
                       ImGui::ColorConvertFloat4ToU32(bg), 6.0f);
    dl->AddRect({toast_x, toast_y}, {toast_x + toast_w, toast_y + toast_h},
                 ImGui::ColorConvertFloat4ToU32(border), 6.0f);

    // title
    ImVec4 title_col = theme.text_bright;
    title_col.w = alpha;
    dl->AddText({toast_x + 10, toast_y + 8},
                 ImGui::ColorConvertFloat4ToU32(title_col), title_.c_str());

    // body, truncated if needed because nobody writes short messages
    ImVec4 body_col = theme.text_default;
    body_col.w = alpha;

    std::string display_body = body_;
    if (display_body.size() > 80) {
        display_body = display_body.substr(0, 77) + "...";
    }
    dl->AddText({toast_x + 10, toast_y + 30},
                 ImGui::ColorConvertFloat4ToU32(body_col), display_body.c_str());
}

float NotificationPopup::currentAlpha() const {
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - show_time_).count();

    if (elapsed > duration_) return 0.0f;

    // quick fade in over 0.15s
    float fade_in = std::min(elapsed / 0.15f, 1.0f);

    // fade out over the last 0.5s
    float remaining = duration_ - elapsed;
    float fade_out = std::min(remaining / 0.5f, 1.0f);

    return fade_in * fade_out;
}

} // namespace conduit::ui
