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

    dl->AddRectFilled(p, {p.x + width, p.y + height},
                       ImGui::ColorConvertFloat4ToU32(theme.bg_status));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // weechat-style status line: [HH:MM] [org(state)] [N unread] [typing...]
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

    ImGui::SetCursorPos({x + 4.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);

    std::string status = "[" + clock_ss.str() + "] ";
    if (connection_state_ == "connected") {
        status += "[" + org_name_ + "]";
    } else {
        status += "[" + org_name_ + ":" + connection_state_ + "]";
    }
    if (unread_count_ > 0) {
        status += " [" + std::to_string(unread_count_) + " unread]";
    }
    ImGui::TextUnformatted(status.c_str());

    if (!typing_users_.empty()) {
        ImGui::SameLine(0, 8.0f);
        std::string typing;
        if (typing_users_.size() == 1) typing = typing_users_[0] + " is typing...";
        else if (typing_users_.size() == 2) typing = typing_users_[0] + " and " + typing_users_[1] + " are typing...";
        else typing = "several people are typing...";
        ImGui::TextUnformatted(typing.c_str());
    }

    ImGui::PopStyleColor();
}

} // namespace conduit::ui
