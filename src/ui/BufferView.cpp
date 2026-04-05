#include "ui/BufferView.h"
#include "render/TextRenderer.h"
#include "render/ReactionBadge.h"
#include <imgui.h>

namespace conduit::ui {

BufferView::BufferView() {}

void BufferView::setMessages(const std::vector<BufferViewMessage>& messages) {
    messages_ = messages;
    has_new_data_ = true;
}

void BufferView::scrollToBottom() {
    auto_scroll_ = true;
}

void BufferView::render(float x, float y, float width, float height, const Theme& theme) {
    ImGui::SetCursorPos({x, y});

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_main);
    ImGui::BeginChild("##bufferview", {width, height}, false);

    const float ts_width = 54.0f;
    const float nick_width = 100.0f;
    const float pad = 8.0f;
    float text_start_x = pad + ts_width + nick_width + 8.0f;
    float text_avail = width - text_start_x - pad;

    if (messages_.empty()) {
        // show something so it's not a void
        ImGui::SetCursorPos({pad + ts_width, height * 0.4f});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("no messages yet. say something, coward.");
        ImGui::PopStyleColor();
    }

    for (auto& msg : messages_) {
        float cursor_x = pad;

        // timestamp
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::SetCursorPosX(cursor_x);
        ImGui::TextUnformatted(msg.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        cursor_x = pad + ts_width;

        // system messages (joins, leaves, bot messages)
        bool is_system = !msg.subtype.empty() && msg.subtype != "bot_message" &&
                         msg.subtype != "me_message";

        if (is_system) {
            ImGui::SetCursorPosX(cursor_x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            std::string sys_text = msg.nick.empty() ? msg.text : msg.nick + " " + msg.text;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_avail + nick_width);
            ImGui::TextUnformatted(sys_text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            continue;
        }

        // nick color (deterministic from user_id)
        ImVec4 nick_color = theme.nickColor(msg.user_id);

        // right-align nick in its column
        float name_width_px = ImGui::CalcTextSize(msg.nick.c_str()).x;
        float nick_x = cursor_x + nick_width - name_width_px;
        ImGui::SetCursorPosX(nick_x);
        ImGui::PushStyleColor(ImGuiCol_Text, nick_color);
        ImGui::TextUnformatted(msg.nick.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // message text with mrkdwn rendering
        ImGui::SetCursorPosX(text_start_x);

        auto spans = render::parseMrkdwn(msg.text);
        if (spans.empty()) {
            // empty message (file-only, etc)
            ImGui::NewLine();
        } else {
            ImGui::PushTextWrapPos(text_start_x + text_avail);
            render::renderSpans(spans, text_avail, theme.text_default);
            ImGui::PopTextWrapPos();
        }

        // edited indicator
        if (msg.is_edited) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            ImGui::TextUnformatted("(edited)");
            ImGui::PopStyleColor();
        }

        // file attachments
        for (auto& f : msg.files) {
            ImGui::SetCursorPosX(text_start_x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
            std::string file_label = "[file: " + f.name + "]";
            if (f.original_w > 0) {
                file_label += " " + std::to_string(f.original_w) + "x" +
                              std::to_string(f.original_h);
            }
            ImGui::TextUnformatted(file_label.c_str());
            ImGui::PopStyleColor();
        }

        // thread indicator
        if (msg.reply_count > 0) {
            ImGui::SetCursorPosX(text_start_x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
            std::string thread_label = std::to_string(msg.reply_count) +
                                       (msg.reply_count == 1 ? " reply" : " replies");
            ImGui::TextUnformatted(thread_label.c_str());
            ImGui::PopStyleColor();
        }

        // reactions
        if (!msg.reactions.empty()) {
            ImGui::SetCursorPosX(text_start_x);
            auto click = render::ReactionBadge::render(msg.reactions, theme, text_avail);
            // click handling would go here - toggle reaction on/off
        }
    }

    // auto-scroll to bottom when new data arrives and we're already at the bottom
    if (has_new_data_) {
        if (auto_scroll_) {
            ImGui::SetScrollHereY(1.0f);
        }
        has_new_data_ = false;
    }

    // track if we're at the bottom for auto-scroll
    auto_scroll_ = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
