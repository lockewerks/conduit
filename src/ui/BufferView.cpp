#include "ui/BufferView.h"
#include "render/TextRenderer.h"
#include "render/ReactionBadge.h"
#include "render/EmojiMap.h"
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 0.0f}); // zero vertical gap between lines
    ImGui::BeginChild("##bufferview", {width, height}, false);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        selected_index_ = -1;
        selected_ts_.clear();
    }

    const float pad = 4.0f;
    // IRC style: "HH:MM <nick> message text"
    // everything left-aligned, every message attributed, no collapsing

    if (messages_.empty()) {
        ImGui::SetCursorPos({pad + 8.0f, height * 0.45f});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("-- no messages --");
        ImGui::PopStyleColor();
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (size_t mi = 0; mi < messages_.size(); mi++) {
        auto& msg = messages_[mi];
        float msg_start_y = ImGui::GetCursorScreenPos().y;

        bool is_failed = (msg.subtype == "send_failed");
        bool is_system = !msg.subtype.empty() && msg.subtype != "bot_message" &&
                         msg.subtype != "me_message" && msg.subtype != "file_share" &&
                         msg.subtype != "thread_broadcast" && !is_failed;

        if (is_system) {
            // system messages: "HH:MM -- nick has joined the channel"
            ImGui::SetCursorPosX(pad);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            std::string line = msg.timestamp + " -- ";
            if (!msg.nick.empty()) line += msg.nick + " ";
            line += msg.text;
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
            continue;
        }

        // IRC format: "HH:MM <nick> message"
        ImGui::SetCursorPosX(pad);

        // timestamp in dim
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(msg.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // " <nick> " with the nick colored
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(" <");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        ImGui::PushStyleColor(ImGuiCol_Text, theme.nickColor(msg.user_id));
        ImGui::TextUnformatted(msg.nick.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("> ");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // quoted parent message for broadcast replies
        if (!msg.reply_parent_nick.empty()) {
            float qx = ImGui::GetCursorPosX();
            ImVec2 qpos = ImGui::GetCursorScreenPos();
            // accent bar
            dl->AddRectFilled(qpos, {qpos.x + 2.0f, qpos.y + ImGui::GetTextLineHeightWithSpacing()},
                               ImGui::ColorConvertFloat4ToU32({0.35f, 0.55f, 0.85f, 0.8f}));
            ImGui::SetCursorPosX(qx + 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.45f, 0.60f, 0.85f, 1.0f});
            ImGui::TextUnformatted(msg.reply_parent_nick.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.50f, 0.52f, 0.55f, 1.0f});
            std::string preview = msg.reply_parent_text;
            if (preview.size() > 80) preview = preview.substr(0, 77) + "...";
            ImGui::TextUnformatted(preview.c_str());
            ImGui::PopStyleColor();
        }

        // message text with mrkdwn — failed sends get the red treatment
        float text_start = ImGui::GetCursorPosX();
        float text_avail = width - text_start - pad;
        ImVec4 msg_color = is_failed ? ImVec4{0.9f, 0.3f, 0.3f, 1.0f} : theme.text_default;
        auto spans = render::parseMrkdwn(msg.text);
        if (!spans.empty()) {
            render::renderSpans(spans, text_avail, msg_color);
        } else if (msg.files.empty()) {
            ImGui::NewLine();
        }

        // edited marker
        if (msg.is_edited) {
            ImGui::SetCursorPosX(text_start);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
            ImGui::TextUnformatted("(edited)");
            ImGui::PopStyleColor();
        }

        // files
        for (auto& f : msg.files) {
            ImGui::SetCursorPosX(text_start);
            bool is_image = (f.mimetype.find("image/") == 0);
            bool is_gif = (f.mimetype == "image/gif");

            if (is_gif) {
                std::string gif_url = f.thumb_360.empty() ? f.url_private : f.thumb_360;
                float max_w = image_renderer_ ? image_renderer_->maxWidth() : 360.0f;
                float max_h = image_renderer_ ? image_renderer_->maxHeight() : 240.0f;
                if (gif_renderer_ && !gif_url.empty() && gif_renderer_->renderInline(gif_url, max_w, max_h)) {
                    continue;
                }
            } else if (is_image) {
                std::string img_url = f.thumb_360.empty() ? f.url_private : f.thumb_360;
                if (image_renderer_ && !img_url.empty() && image_renderer_->renderInline(img_url)) {
                    if (ImGui::IsItemClicked()) {
                        last_image_click_ = {true, img_url};
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                    continue;
                }

                // placeholder while loading
                float img_w = (f.original_w > 0) ? std::min((float)f.original_w, 320.0f) : 200.0f;
                float img_h = (f.original_h > 0 && f.original_w > 0)
                    ? img_w * ((float)f.original_h / (float)f.original_w) : 150.0f;
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
                ImGui::Dummy({img_w, img_h + 2.0f});
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
                ImGui::TextUnformatted(("[" + f.name + "]").c_str());
                ImGui::PopStyleColor();
            }
        }

        // thread replies - clickable to open thread panel
        if (msg.reply_count > 0) {
            ImGui::SetCursorPosX(text_start);
            std::string thr = "\xf0\x9f\x92\xac " + std::to_string(msg.reply_count) +
                               (msg.reply_count == 1 ? " reply" : " replies");
            ImVec2 thr_size = ImGui::CalcTextSize(thr.c_str());
            ImVec2 thr_pos = ImGui::GetCursorScreenPos();
            bool thr_hovered = ImGui::IsMouseHoveringRect(thr_pos, {thr_pos.x + thr_size.x, thr_pos.y + thr_size.y});
            ImGui::PushStyleColor(ImGuiCol_Text, thr_hovered ? theme.url_color : ImVec4{0.35f, 0.55f, 0.85f, 1.0f});
            ImGui::TextUnformatted(thr.c_str());
            ImGui::PopStyleColor();
            if (thr_hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsMouseClicked(0)) {
                    last_thread_click_ = {true, msg.ts};
                }
            }
        }

        // reactions
        if (!msg.reactions.empty()) {
            ImGui::SetCursorPosX(text_start);
            auto click = render::ReactionBadge::render(msg.reactions, theme, text_avail, emoji_renderer_);
            if (click.clicked) {
                last_reaction_click_ = {true, click.emoji_name, msg.ts};
            }
        }

        float msg_end_y = ImGui::GetCursorScreenPos().y;

        // click to select
        ImVec2 win_pos = ImGui::GetWindowPos();
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            ImVec2 mouse = ImGui::GetMousePos();
            if (mouse.y >= msg_start_y && mouse.y < msg_end_y) {
                selected_index_ = (int)mi;
                selected_ts_ = msg.ts;
            }
        }

        // right-click context menu - the fun stuff
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1)) {
            ImVec2 mouse = ImGui::GetMousePos();
            if (mouse.y >= msg_start_y && mouse.y < msg_end_y) {
                selected_index_ = (int)mi;
                selected_ts_ = msg.ts;
                context_msg_ts_ = msg.ts;
                context_msg_user_ = msg.user_id;
                context_msg_text_ = msg.text;
                ImGui::OpenPopup("##msg_context");
            }
        }

        // hover highlight + floating action bar
        if (ImGui::IsWindowHovered()) {
            ImVec2 mouse = ImGui::GetMousePos();
            bool msg_hovered = (mouse.y >= msg_start_y && mouse.y < msg_end_y);

            if (msg_hovered && (int)mi != selected_index_) {
                dl->AddRectFilled(
                    {win_pos.x, msg_start_y},
                    {win_pos.x + width, msg_end_y},
                    ImGui::ColorConvertFloat4ToU32({0.08f, 0.08f, 0.12f, 0.5f}));
            }

            // floating action bar at top-right of hovered message
            if (msg_hovered && !is_system) {
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                float bar_h = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
                float pad = 6.0f;

                // measure button labels
                const char* react_label = "React";
                const char* reply_label = "Reply";
                const char* more_label = "...";
                ImVec2 react_sz = ImGui::CalcTextSize(react_label);
                ImVec2 reply_sz = ImGui::CalcTextSize(reply_label);
                ImVec2 more_sz = ImGui::CalcTextSize(more_label);
                float react_w = react_sz.x + pad * 2;
                float reply_w = reply_sz.x + pad * 2;
                float more_w = more_sz.x + pad * 2;
                float gap = 2.0f;
                float bar_w = react_w + reply_w + more_w + gap * 2 + pad * 2;

                float bar_x = win_pos.x + width - bar_w - 16.0f;
                float bar_y = msg_start_y - bar_h * 0.5f;
                if (bar_y < win_pos.y) bar_y = msg_start_y;

                // bar background with border
                fg->AddRectFilled(
                    {bar_x, bar_y}, {bar_x + bar_w, bar_y + bar_h},
                    ImGui::ColorConvertFloat4ToU32({0.12f, 0.12f, 0.16f, 0.97f}), 5.0f);
                fg->AddRect(
                    {bar_x, bar_y}, {bar_x + bar_w, bar_y + bar_h},
                    ImGui::ColorConvertFloat4ToU32({0.30f, 0.30f, 0.36f, 1.0f}), 5.0f);

                ImU32 text_normal = ImGui::ColorConvertFloat4ToU32({0.72f, 0.75f, 0.80f, 1.0f});
                ImU32 text_bright = ImGui::ColorConvertFloat4ToU32({0.90f, 0.92f, 0.95f, 1.0f});
                ImU32 bg_hover = ImGui::ColorConvertFloat4ToU32({0.22f, 0.24f, 0.32f, 1.0f});
                float text_y = bar_y + (bar_h - react_sz.y) * 0.5f;
                float bx = bar_x + pad;

                // helper lambda for each button
                auto drawBtn = [&](const char* label, ImVec2 sz, float w,
                                   auto on_click) {
                    bool hov = (mouse.x >= bx && mouse.x < bx + w &&
                                mouse.y >= bar_y && mouse.y < bar_y + bar_h);
                    if (hov) {
                        fg->AddRectFilled({bx, bar_y + 2}, {bx + w, bar_y + bar_h - 2},
                                           bg_hover, 3.0f);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        if (ImGui::IsMouseClicked(0)) on_click();
                    }
                    fg->AddText({bx + pad, text_y}, hov ? text_bright : text_normal, label);
                    bx += w + gap;
                };

                drawBtn(react_label, react_sz, react_w, [&]() {
                    last_context_action_ = {ContextAction::React, msg.ts, msg.text};
                });
                drawBtn(reply_label, reply_sz, reply_w, [&]() {
                    last_context_action_ = {ContextAction::Reply, msg.ts, msg.text};
                });
                drawBtn(more_label, more_sz, more_w, [&]() {
                    context_msg_ts_ = msg.ts;
                    context_msg_user_ = msg.user_id;
                    context_msg_text_ = msg.text;
                    ImGui::OpenPopup("##msg_context");
                });
            }
        }

        // selection highlight
        if ((int)mi == selected_index_) {
            dl->AddRectFilled(
                {win_pos.x, msg_start_y},
                {win_pos.x + width, msg_end_y},
                ImGui::ColorConvertFloat4ToU32(
                    {theme.bg_selected.x, theme.bg_selected.y, theme.bg_selected.z, 0.3f}));
        }
    }

    // message context menu popup - rendered once, not per-message
    if (ImGui::BeginPopup("##msg_context")) {
        // quick reaction row: top 6 emoji + "more" button
        {
            static const struct { const char* name; const char* fallback; } quick_emoji[] = {
                {"thumbsup",      "+1"},
                {"heart",         "<3"},
                {"joy",           "XD"},
                {"eyes",          "OO"},
                {"fire",          "**"},
                {"raised_hands",  "!!"},
            };

            auto& emap = conduit::render::getEmojiMap();
            float btn_size = ImGui::GetTextLineHeightWithSpacing() + 8.0f;

            for (int i = 0; i < 6; i++) {
                if (i > 0) ImGui::SameLine(0, 2);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.15f, 0.20f, 1.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.25f, 0.25f, 0.35f, 1.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.30f, 0.30f, 0.45f, 1.0f});

                std::string btn_id = "##qr_" + std::to_string(i);
                auto it = emap.find(quick_emoji[i].name);
                std::string label = (it != emap.end()) ? it->second : quick_emoji[i].fallback;
                label += btn_id;

                if (ImGui::Button(label.c_str(), {btn_size, btn_size})) {
                    last_context_action_ = {ContextAction::QuickReact, context_msg_ts_,
                                            context_msg_text_, quick_emoji[i].name};
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(":%s:", quick_emoji[i].name);
                }
                ImGui::PopStyleColor(3);
            }

            // "+" button to open the full emoji picker
            ImGui::SameLine(0, 2);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.12f, 0.12f, 0.16f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.22f, 0.22f, 0.32f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.28f, 0.28f, 0.40f, 1.0f});
            if (ImGui::Button("+##qr_more", {btn_size, btn_size})) {
                last_context_action_ = {ContextAction::React, context_msg_ts_, context_msg_text_};
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("more reactions...");
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Copy text")) {
            last_context_action_ = {ContextAction::Copy, context_msg_ts_, context_msg_text_};
        }
        if (ImGui::MenuItem("Reply in thread")) {
            last_context_action_ = {ContextAction::Reply, context_msg_ts_, context_msg_text_};
        }

        // edit/delete only for your own messages
        bool is_own = (!self_user_id_.empty() && context_msg_user_ == self_user_id_);
        if (is_own) {
            ImGui::Separator();
            if (ImGui::MenuItem("Edit")) {
                last_context_action_ = {ContextAction::Edit, context_msg_ts_, context_msg_text_};
            }
            if (ImGui::MenuItem("Delete")) {
                last_context_action_ = {ContextAction::Delete, context_msg_ts_, context_msg_text_};
            }
        }
        ImGui::EndPopup();
    }

    // auto-scroll
    if (has_new_data_ && auto_scroll_) {
        ImGui::SetScrollHereY(1.0f);
        has_new_data_ = false;
    }
    auto_scroll_ = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f);

    // "new messages" jump button - floats at the bottom when you've scrolled up
    if (!auto_scroll_) {
        float btn_w = ImGui::CalcTextSize("[new messages]").x + 16.0f;
        ImGui::SetCursorPos({(width - btn_w) * 0.5f, height - 24.0f});
        ImGui::PushStyleColor(ImGuiCol_Button, {0.10f, 0.10f, 0.15f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.14f, 0.14f, 0.20f, 0.95f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.18f, 0.18f, 0.26f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_Text, theme.url_color);
        if (ImGui::Button("[new messages]")) {
            auto_scroll_ = true;
            has_new_data_ = true;
        }
        ImGui::PopStyleColor(4);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
