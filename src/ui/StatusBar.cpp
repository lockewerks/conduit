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

    // dark strip across the bottom, the floor of our little world
    dl->AddRectFilled(p, {p.x + width, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.bg_status));

    // thin top border to separate from the input area
    dl->AddLine(p, {p.x + width, p.y}, ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    float text_y = y + (height - ImGui::GetTextLineHeight()) * 0.5f;

    // connection status dot: green/yellow/red depending on state
    ImVec4 dot_color;
    if (connection_state_ == "connected") {
        dot_color = {0.30f, 0.85f, 0.40f, 1.0f}; // green, all systems go
    } else if (connection_state_.find("reconnect") != std::string::npos ||
               connection_state_.find("connecting") != std::string::npos) {
        dot_color = {0.95f, 0.80f, 0.20f, 1.0f}; // yellow, hold tight
    } else {
        dot_color = {0.90f, 0.30f, 0.30f, 1.0f}; // red, we have a problem
    }

    ImGui::SetCursorPos({x + 8.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, dot_color);
    ImGui::TextUnformatted("\xe2\x97\x8f"); // "●"
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6.0f);

    // org name and connection state
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    std::string status_text = org_name_;
    if (connection_state_ != "connected") {
        status_text += " \xe2\x80\xa2 " + connection_state_; // " · state"
    }
    ImGui::TextUnformatted(status_text.c_str());

    // unread badge if anything is piling up
    if (unread_count_ > 0) {
        ImGui::SameLine(0, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.mention_badge);
        std::string unreads = std::to_string(unread_count_) + " unread";
        ImGui::TextUnformatted(unreads.c_str());
        ImGui::PopStyleColor();
    }

    // typing indicator - who's mashing their keyboard right now
    if (!typing_users_.empty()) {
        ImGui::SameLine(0, 12.0f);
        std::string typing;
        if (typing_users_.size() == 1) {
            typing = typing_users_[0] + " is typing...";
        } else if (typing_users_.size() == 2) {
            typing = typing_users_[0] + " and " + typing_users_[1] + " are typing...";
        } else {
            typing = "several people are typing...";
        }
        ImGui::TextUnformatted(typing.c_str());
    }

    ImGui::PopStyleColor(); // text_dim

    // clock on the right because what's a status bar without a clock
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
    ImGui::SetCursorPos({x + width - clock_width - 12.0f, text_y});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted(clock_str.c_str());
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
