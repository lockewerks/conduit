#include "ui/BufferView.h"
#include <imgui.h>

namespace conduit::ui {

BufferView::BufferView() {
    // placeholder messages so we can see the layout working
    // if you're reading this and expecting real slack data, patience grasshopper
    messages_ = {
        {"09:00", "alice", "good morning everyone"},
        {"09:01", "bob", "morning! anyone else's slack client using 2GB of RAM?"},
        {"09:01", "carol", "just electron things"},
        {"09:02", "alice", "have you tried turning it off and never on again"},
        {"09:03", "dave", "shipped the new API yesterday, seems stable so far"},
        {"09:03", "dave", "famous last words, I know"},
        {"09:05", "", "--- new messages ---", true},
        {"09:10", "alice", "hey @bob did you see the PR for the auth refactor?"},
        {"09:11", "bob", "yeah looks good, left a few comments"},
        {"09:12", "carol", "the CI is green for once, screenshot for posterity"},
        {"09:12", "carol", "[image: ci_green.png]"},
        {"09:15", "bot", "Build #4567 passed on main", true},
        {"09:16", "dave", "let's not jinx it"},
        {"09:20", "alice", "too late, prod is on fire"},
        {"09:20", "alice", "jk jk"},
        {"09:21", "bob", ":laughing: :fire:"},
        {"09:22", "carol", "one of these days we'll have a boring standup"},
        {"09:23", "dave", "that day is not today"},
        {"09:30", "alice", "alright, standup time. what's everyone working on?"},
        {"09:30", "bob", "still fighting the rate limiter. slack's API is... special."},
        {"09:31", "carol", "frontend stuff. CSS is pain."},
        {"09:31", "dave", "trying to make libwebsockets behave on windows"},
        {"09:32", "alice", "godspeed dave"},
    };
}

void BufferView::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_main);
    ImGui::BeginChild("##bufferview", {width, height}, false);

    const float ts_width = 54.0f;   // timestamp column
    const float nick_width = 100.0f; // nickname column (right-aligned)
    const float pad = 8.0f;

    for (const auto& msg : messages_) {
        float cursor_x = pad;

        // timestamp in dim
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::SetCursorPosX(cursor_x);
        ImGui::TextUnformatted(msg.time.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        cursor_x = pad + ts_width;

        if (msg.is_system && msg.nick.empty()) {
            // system message (separator line, bot messages, etc)
            ImGui::SetCursorPosX(cursor_x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.separator_line);
            ImGui::TextUnformatted(msg.text.c_str());
            ImGui::PopStyleColor();
        } else {
            // nick in its color (deterministic based on name)
            ImVec4 nick_color = theme.nickColor(msg.nick);
            if (msg.is_system) {
                nick_color = theme.text_dim;
            }

            // right-align the nickname in its column
            float name_width_px = ImGui::CalcTextSize(msg.nick.c_str()).x;
            float nick_x = cursor_x + nick_width - name_width_px;
            ImGui::SetCursorPosX(nick_x);
            ImGui::PushStyleColor(ImGuiCol_Text, nick_color);
            ImGui::TextUnformatted(msg.nick.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);

            // message text
            ImGui::SetCursorPosX(cursor_x + nick_width + 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);

            // wrap text to fit available width
            float text_start = cursor_x + nick_width + 8.0f;
            float avail = width - text_start - pad;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail);
            ImGui::TextUnformatted(msg.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
    }

    // auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
