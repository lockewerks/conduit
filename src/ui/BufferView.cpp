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
    // tight spacing inside the chat area - this is a terminal, not a word processor
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 1.0f});
    ImGui::BeginChild("##bufferview", {width, height}, false);

    // weechat-style column layout
    // [HH:MM] [  nickname] | message text here
    const float pad = 8.0f;
    const float ts_width = 42.0f;     // "HH:MM" in monospace
    const float nick_width = 110.0f;  // right-aligned nick column
    const float sep_gap = 8.0f;       // gap after nick
    const float text_x = pad + ts_width + nick_width + sep_gap;
    const float text_avail = std::max(100.0f, width - text_x - pad);

    if (messages_.empty()) {
        float center_y = height * 0.45f;
        ImGui::SetCursorPos({text_x, center_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("no messages. type something below.");
        ImGui::PopStyleColor();
    }

    std::string last_nick; // collapse repeated nicks like weechat

    for (size_t mi = 0; mi < messages_.size(); mi++) {
        auto& msg = messages_[mi];
        bool is_system = !msg.subtype.empty() && msg.subtype != "bot_message" &&
                         msg.subtype != "me_message";

        // timestamp column
        ImGui::SetCursorPosX(pad);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(msg.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        if (is_system) {
            // system messages span the full width after timestamp
            ImGui::SetCursorPosX(pad + ts_width + 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            std::string sys = msg.nick.empty() ? msg.text : msg.nick + " " + msg.text;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_avail + nick_width);
            ImGui::TextUnformatted(sys.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            continue;
        }

        // nick column - right-aligned, colored by user id hash
        // collapse repeated nicks (show blank for consecutive messages from same user)
        bool show_nick = (msg.nick != last_nick);
        last_nick = msg.nick;

        if (show_nick) {
            ImVec4 nick_color = theme.nickColor(msg.user_id);
            float name_w = ImGui::CalcTextSize(msg.nick.c_str()).x;
            float nick_x = pad + ts_width + nick_width - name_w;
            ImGui::SetCursorPosX(nick_x);
            ImGui::PushStyleColor(ImGuiCol_Text, nick_color);
            ImGui::TextUnformatted(msg.nick.c_str());
            ImGui::PopStyleColor();
        } else {
            // blank nick for consecutive messages from same person
            ImGui::SetCursorPosX(pad + ts_width + nick_width);
            ImGui::TextUnformatted(""); // need something so SameLine works
        }
        ImGui::SameLine(0, 0);

        // message text with mrkdwn rendering
        ImGui::SetCursorPosX(text_x);
        auto spans = render::parseMrkdwn(msg.text);
        if (!spans.empty()) {
            render::renderSpans(spans, text_avail, theme.text_default);
        }

        // edited indicator
        if (msg.is_edited) {
            ImGui::SetCursorPosX(text_x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            ImGui::TextUnformatted("(edited)");
            ImGui::PopStyleColor();
        }

        // file attachments - now with inline image placeholders for the visual thinkers
        for (auto& f : msg.files) {
            ImGui::SetCursorPosX(text_x);

            bool is_image = (f.mimetype.find("image/") == 0);

            if (is_image) {
                // inline image placeholder with a visible bounding box
                // so it looks intentional and not like a missing texture
                float img_w = (f.original_w > 0) ? std::min((float)f.original_w, 320.0f) : 320.0f;
                float img_h = (f.original_h > 0)
                    ? img_w * ((float)f.original_h / (float)f.original_w)
                    : 240.0f;
                img_h = std::min(img_h, 240.0f);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cursor = ImGui::GetCursorScreenPos();

                // dark box with a subtle border
                dl->AddRectFilled(cursor, {cursor.x + img_w, cursor.y + img_h},
                                  ImGui::ColorConvertFloat4ToU32(theme.code_bg));
                dl->AddRect(cursor, {cursor.x + img_w, cursor.y + img_h},
                            ImGui::ColorConvertFloat4ToU32(theme.separator_line));

                // centered label inside the box
                std::string label = "[img: " + f.name;
                if (f.original_w > 0) {
                    label += " " + std::to_string(f.original_w) + "x" + std::to_string(f.original_h);
                }
                label += "]";

                ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
                float label_x = cursor.x + (img_w - label_size.x) * 0.5f;
                float label_y = cursor.y + (img_h - label_size.y) * 0.5f;

                dl->AddText({label_x, label_y},
                            ImGui::ColorConvertFloat4ToU32(theme.url_color),
                            label.c_str());

                ImGui::Dummy({img_w, img_h + 4.0f});
            } else {
                // regular file attachment, just show the name
                ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
                std::string label = "\xf0\x9f\x93\x8e " + f.name; // paperclip emoji vibes
                if (f.original_w > 0) {
                    label += " " + std::to_string(f.original_w) + "x" + std::to_string(f.original_h);
                }
                ImGui::TextUnformatted(label.c_str());
                ImGui::PopStyleColor();
            }
        }

        // thread reply count
        if (msg.reply_count > 0) {
            ImGui::SetCursorPosX(text_x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
            std::string thr = std::to_string(msg.reply_count) +
                               (msg.reply_count == 1 ? " reply" : " replies");
            ImGui::TextUnformatted(thr.c_str());
            ImGui::PopStyleColor();
        }

        // reactions
        if (!msg.reactions.empty()) {
            ImGui::SetCursorPosX(text_x);
            render::ReactionBadge::render(msg.reactions, theme, text_avail);
        }

        // tiny gap between messages
        ImGui::Dummy({0, 1.0f});
    }

    // auto-scroll
    if (has_new_data_ && auto_scroll_) {
        ImGui::SetScrollHereY(1.0f);
        has_new_data_ = false;
    }

    auto_scroll_ = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f);

    ImGui::EndChild();
    ImGui::PopStyleVar(); // ItemSpacing
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
