#include "ui/StatusBar.h"
#include <imgui.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace conduit::ui {

void StatusBar::render(float x, float y, float width, float height, const Theme& theme) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.y = ImGui::GetWindowPos().y + y;
    p.x = ImGui::GetWindowPos().x + x;

    // dark background strip
    dl->AddRectFilled(p, {p.x + width, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.bg_status));

    ImGui::SetCursorPos({x + 8.0f, y + 2.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);

    // [conduit] [org] [status] [unreads]
    std::string left = "[conduit] [" + org_name_ + "] [" + connection_state_ + "]";
    if (unread_count_ > 0) {
        left += " [" + std::to_string(unread_count_) + " unread]";
    }
    ImGui::TextUnformatted(left.c_str());

    // clock on the right side because every good status bar has a clock
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    std::ostringstream clock_ss;
    clock_ss << std::put_time(&tm_buf, "%H:%M");
    std::string clock_str = clock_ss.str();

    float clock_width = ImGui::CalcTextSize(clock_str.c_str()).x;
    ImGui::SetCursorPos({x + width - clock_width - 12.0f, y + 2.0f});
    ImGui::TextUnformatted(clock_str.c_str());

    ImGui::PopStyleColor();
}

} // namespace conduit::ui
