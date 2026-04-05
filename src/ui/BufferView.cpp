#include "ui/BufferView.h"
#include "render/TextRenderer.h"
#include "render/ReactionBadge.h"
#include <imgui.h>
#include <algorithm>

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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 1.0f});
    ImGui::BeginChild("##bufferview", {width, height}, false);

    // escape clears message selection
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        selected_index_ = -1;
        selected_ts_.clear();
    }

    const float pad = 8.0f;
    const float ts_width = 42.0f;
    const float nick_width = 110.0f;
    const float sep_gap = 8.0f;
    const float text_x = pad + ts_width + nick_width + sep_gap;
    const float text_avail = std::max(100.0f, width - text_x - pad);

    if (messages_.empty()) {
        float center_y = height * 0.45f;
        ImGui::SetCursorPos({text_x, center_y});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("no messages. type something below.");
        ImGui::PopStyleColor();
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::string last_nick;

    for (size_t mi = 0; mi < messages_.size(); mi++) {
        auto& msg = messages_[mi];
        bool is_system = !msg.subtype.empty() && msg.subtype != "bot_message" &&
                         msg.subtype != "me_message";

        // track Y position for click detection and selection highlight
        float msg_start_y = ImGui::GetCursorScreenPos().y;

        // timestamp
        ImGui::SetCursorPosX(pad);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(msg.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        if (is_system) {
            ImGui::SetCursorPosX(pad + ts_width + 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            std::string sys = msg.nick.empty() ? msg.text : msg.nick + " " + msg.text;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_avail + nick_width);
            ImGui::TextUnformatted(sys.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            continue;
        }

        // nick (collapsed for consecutive messages from same person)
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
            ImGui::SetCursorPosX(pad + ts_width + nick_width);
            ImGui::TextUnformatted("");
        }
        ImGui::SameLine(0, 0);

        // message text
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

        // file attachments
        for (auto& f : msg.files) {
            ImGui::SetCursorPosX(text_x);
            bool is_image = (f.mimetype.find("image/") == 0);
            bool is_gif = (f.mimetype == "image/gif");

            if (is_gif) {
                // GIFs get the special treatment: animated frames, the whole shebang
                std::string gif_url = f.thumb_360.empty() ? f.url_private : f.thumb_360;
                float max_w = image_renderer_ ? image_renderer_->maxWidth() : 360.0f;
                float max_h = image_renderer_ ? image_renderer_->maxHeight() : 240.0f;
                if (gif_renderer_ && !gif_url.empty() && gif_renderer_->renderInline(gif_url, max_w, max_h)) {
                    continue;
                }
                // still loading, show the placeholder box below
            } else if (is_image) {
                // try to render the actual image if we have a renderer wired up
                std::string img_url = f.thumb_360.empty() ? f.url_private : f.thumb_360;
                if (image_renderer_ && !img_url.empty() && image_renderer_->renderInline(img_url)) {
                    continue;
                }

                // fallback placeholder box while loading
                float img_w = (f.original_w > 0) ? std::min((float)f.original_w, 320.0f) : 200.0f;
                float img_h = (f.original_h > 0 && f.original_w > 0)
                    ? img_w * ((float)f.original_h / (float)f.original_w)
                    : 150.0f;
                img_h = std::min(img_h, 240.0f);

                ImVec2 cursor = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(cursor, {cursor.x + img_w, cursor.y + img_h},
                                  ImGui::ColorConvertFloat4ToU32(theme.code_bg));
                dl->AddRect(cursor, {cursor.x + img_w, cursor.y + img_h},
                            ImGui::ColorConvertFloat4ToU32(theme.separator_line));

                std::string label = "[loading: " + f.name + "]";
                ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
                dl->AddText({cursor.x + (img_w - label_size.x) * 0.5f,
                             cursor.y + (img_h - label_size.y) * 0.5f},
                            ImGui::ColorConvertFloat4ToU32(theme.text_dim), label.c_str());
                ImGui::Dummy({img_w, img_h + 4.0f});
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
                std::string label = "\xf0\x9f\x93\x8e " + f.name;
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

        // reactions - click a badge to toggle your own reaction
        if (!msg.reactions.empty()) {
            ImGui::SetCursorPosX(text_x);
            auto click = render::ReactionBadge::render(msg.reactions, theme, text_avail);
            if (click.clicked) {
                last_reaction_click_ = {true, click.emoji_name, msg.ts};
            }
        }

        float msg_end_y = ImGui::GetCursorScreenPos().y;

        // click to select this message (for reactions, edit, delete, thread)
        ImVec2 win_pos = ImGui::GetWindowPos();
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            ImVec2 mouse = ImGui::GetMousePos();
            if (mouse.y >= msg_start_y && mouse.y < msg_end_y) {
                selected_index_ = (int)mi;
                selected_ts_ = msg.ts;
            }
        }

        // selection highlight
        if ((int)mi == selected_index_) {
            dl->AddRectFilled(
                {win_pos.x, msg_start_y},
                {win_pos.x + width, msg_end_y},
                ImGui::ColorConvertFloat4ToU32(
                    {theme.bg_selected.x, theme.bg_selected.y, theme.bg_selected.z, 0.35f}));
        }

        ImGui::Dummy({0, 1.0f});
    }

    // auto-scroll
    if (has_new_data_ && auto_scroll_) {
        ImGui::SetScrollHereY(1.0f);
        has_new_data_ = false;
    }
    auto_scroll_ = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
