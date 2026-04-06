#include "ui/ThreadPanel.h"
#include "render/TextRenderer.h"
#include "util/TimeFormat.h"
#include <imgui.h>

namespace conduit::ui {

void ThreadPanel::open(const slack::ChannelId& channel, const slack::Timestamp& parent_ts) {
    channel_id_ = channel;
    parent_ts_ = parent_ts;
    is_open_ = true;
    std::memset(reply_buf_, 0, sizeof(reply_buf_));
    focus_reply_ = true;
}

void ThreadPanel::close() {
    is_open_ = false;
    messages_.clear();
    std::memset(reply_buf_, 0, sizeof(reply_buf_));
}

void ThreadPanel::addMessage(const slack::Message& message) {
    // insert in order
    auto pos = std::lower_bound(messages_.begin(), messages_.end(), message,
                                 [](const slack::Message& a, const slack::Message& b) {
                                     return a.ts < b.ts;
                                 });
    if (pos != messages_.end() && pos->ts == message.ts) {
        *pos = message;
    } else {
        messages_.insert(pos, message);
    }
}

void ThreadPanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetCursorPos({x, y});

    // separator line between main view and thread
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine(p, {p.x, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_main);
    ImGui::BeginChild("##thread_panel", {width, height}, false);

    float input_height = ImGui::GetTextLineHeightWithSpacing() * 2 + 8.0f;
    float messages_height = height - input_height - 30.0f; // 30 for header

    // header with close button
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted("  Thread");
    ImGui::PopStyleColor();
    ImGui::SameLine(width - 24.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    if (ImGui::SmallButton("X##close_thread")) {
        close();
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }
    ImGui::PopStyleColor();
    ImGui::Separator();

    // scrollable message area
    ImGui::BeginChild("##thread_msgs", {0, messages_height}, false);

    for (auto& msg : messages_) {
        float pad = 4.0f;
        ImGui::SetCursorPosX(pad);

        // timestamp
        std::string time_str = util::formatTime(msg.ts);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(time_str.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // resolve display name
        std::string nick = msg.user;
        if (display_name_fn_) {
            nick = display_name_fn_(msg.user);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(" <");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        ImGui::PushStyleColor(ImGuiCol_Text, theme.nickColor(msg.user));
        ImGui::TextUnformatted(nick.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("> ");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // message text with mrkdwn
        float text_start = ImGui::GetCursorPosX();
        float text_avail = width - text_start - pad;
        auto spans = render::parseMrkdwn(msg.text);
        if (!spans.empty()) {
            render::renderSpans(spans, text_avail, theme.text_default);
        } else {
            ImGui::NewLine();
        }
    }

    if (messages_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("  No replies yet.");
        ImGui::PopStyleColor();
    }

    // auto-scroll to bottom when new messages arrive
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    // reply input box
    ImGui::Separator();
    ImGui::SetCursorPosX(4.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
    ImGui::TextUnformatted("  Reply:");
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(4.0f);
    ImGui::PushItemWidth(width - 12.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{0.08f, 0.08f, 0.10f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);

    if (focus_reply_) {
        ImGui::SetKeyboardFocusHere();
        focus_reply_ = false;
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputText("##thread_reply", reply_buf_, sizeof(reply_buf_), flags)) {
        std::string text(reply_buf_);
        if (!text.empty() && reply_cb_) {
            reply_cb_(channel_id_, parent_ts_, text);
            std::memset(reply_buf_, 0, sizeof(reply_buf_));
            focus_reply_ = true;
        }
    }

    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
