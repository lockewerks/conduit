#include "ui/StatusBar.h"
#include <imgui.h>
#include <chrono>
#include <cmath>
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

    // clock segment
    std::string clock_str = "[" + clock_ss.str() + "] ";
    ImGui::TextUnformatted(clock_str.c_str());
    ImGui::SameLine(0, 0);

    // connection dot — green pulse for connected, yellow blink for reconnecting, red blink for dead
    if (connection_state_ == "connected") {
        float pulse = 0.7f + 0.3f * sinf((float)ImGui::GetTime() * 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, {0.2f, 0.9f * pulse, 0.2f, 1.0f});
        ImGui::TextUnformatted("\xe2\x97\x8f"); // filled circle
        ImGui::PopStyleColor();
    } else if (connection_state_ == "reconnecting") {
        float blink = fmodf((float)ImGui::GetTime() * 2.0f, 2.0f) < 1.0f ? 1.0f : 0.3f;
        ImGui::PushStyleColor(ImGuiCol_Text, {0.9f, 0.8f, 0.1f, blink});
        ImGui::TextUnformatted("\xe2\x97\x8b"); // hollow circle
        ImGui::PopStyleColor();
    } else {
        float blink = fmodf((float)ImGui::GetTime() * 3.0f, 2.0f) < 1.0f ? 1.0f : 0.3f;
        ImGui::PushStyleColor(ImGuiCol_Text, {0.9f, 0.2f, 0.2f, blink});
        ImGui::TextUnformatted("\xe2\x97\x8b"); // hollow circle
        ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 0);

    // org name bracket
    if (connection_state_ == "connected") {
        std::string org_str = "[" + org_name_ + "]";
        ImGui::TextUnformatted(org_str.c_str());
    } else {
        std::string org_str = "[" + org_name_ + ":" + connection_state_ + "]";
        ImGui::TextUnformatted(org_str.c_str());
    }

    if (unread_count_ > 0) {
        ImGui::SameLine(0, 4.0f);
        std::string unread_str = "[" + std::to_string(unread_count_) + " unread]";
        ImGui::TextUnformatted(unread_str.c_str());
    }

    // animated typing indicator — cycle dots so it looks alive
    if (!typing_users_.empty()) {
        ImGui::SameLine(0, 8.0f);
        int dots = ((int)(ImGui::GetTime() * 2.0)) % 4;
        std::string dot_str(dots, '.');
        std::string typing;
        if (typing_users_.size() == 1)
            typing = typing_users_[0] + " is typing" + dot_str;
        else if (typing_users_.size() == 2)
            typing = typing_users_[0] + " and " + typing_users_[1] + " are typing" + dot_str;
        else
            typing = "several people are typing" + dot_str;
        ImGui::TextUnformatted(typing.c_str());
    }

    ImGui::PopStyleColor();
}

} // namespace conduit::ui
