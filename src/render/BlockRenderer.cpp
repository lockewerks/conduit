#include "render/BlockRenderer.h"
#include "render/TextRenderer.h"
#include "util/Logger.h"

namespace conduit::render {

float BlockRenderer::render(const std::vector<slack::Block>& blocks, float wrap_width,
                             const ImVec4& default_color) {
    float start_y = ImGui::GetCursorPosY();

    for (auto& block : blocks) {
        if (block.type == "rich_text") {
            renderRichText(block, wrap_width, default_color);
        } else if (block.type == "section") {
            renderSection(block, wrap_width, default_color);
        } else if (block.type == "header") {
            renderHeader(block);
        } else if (block.type == "divider") {
            renderDivider(wrap_width);
        } else if (block.type == "context") {
            renderContext(block, wrap_width);
        } else if (block.type == "image") {
            renderImage(block, wrap_width);
        } else if (block.type == "actions") {
            renderActions(block);
        }
        // small gap between blocks
        ImGui::Dummy(ImVec2(0, 2));
    }

    return ImGui::GetCursorPosY() - start_y;
}

void BlockRenderer::renderRichText(const slack::Block& block, float wrap_width,
                                    const ImVec4& color) {
    for (auto& element : block.elements) {
        if (element.type == "rich_text_section") {
            renderRichTextSection(element, wrap_width, color);
        } else if (element.type == "rich_text_preformatted") {
            // code block
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float padding = 4.0f;
            ImGui::Indent(padding);

            auto* dl = ImGui::GetWindowDrawList();
            float block_start_y = ImGui::GetCursorScreenPos().y;

            for (auto& child : element.elements) {
                if (!child.text.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.5f, 1.0f));
                    ImGui::TextWrapped("%s", child.text.c_str());
                    ImGui::PopStyleColor();
                }
            }

            float block_end_y = ImGui::GetCursorScreenPos().y;
            dl->AddRectFilled(
                ImVec2(pos.x, block_start_y - 2),
                ImVec2(pos.x + wrap_width, block_end_y + 2),
                IM_COL32(30, 30, 30, 200), 3.0f);

            ImGui::Unindent(padding);
        } else if (element.type == "rich_text_quote") {
            // blockquote with green bar
            ImVec2 pos = ImGui::GetCursorScreenPos();
            auto* dl = ImGui::GetWindowDrawList();
            float quote_start_y = pos.y;

            ImGui::Indent(12.0f);
            for (auto& child : element.elements) {
                if (!child.text.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.6f, 1.0f));
                    ImGui::TextWrapped("%s", child.text.c_str());
                    ImGui::PopStyleColor();
                }
            }
            float quote_end_y = ImGui::GetCursorScreenPos().y;
            dl->AddRectFilled(
                ImVec2(pos.x, quote_start_y),
                ImVec2(pos.x + 3, quote_end_y),
                IM_COL32(80, 180, 80, 255));
            ImGui::Unindent(12.0f);
        } else if (element.type == "rich_text_list") {
            int idx = 1;
            for (auto& item : element.elements) {
                std::string bullet = (element.type == "rich_text_list" && element.value == "ordered")
                                     ? std::to_string(idx++) + ". "
                                     : "  * ";
                ImGui::TextUnformatted(bullet.c_str());
                ImGui::SameLine();
                renderRichTextSection(item, wrap_width - 20.0f, color);
            }
        }
    }
}

void BlockRenderer::renderRichTextSection(const slack::BlockElement& section, float wrap_width,
                                           const ImVec4& color) {
    // build styled spans from the rich_text_section elements
    bool first = true;
    for (auto& el : section.elements) {
        if (el.type == "text") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;

            ImVec4 text_color = color;
            bool pop_font = false;

            if (el.bold) text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (el.italic) text_color = ImVec4(0.6f, 0.7f, 0.9f, 1.0f);
            if (el.strike) text_color.w = 0.5f;

            if (el.code) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.5f, 1.0f));
                ImGui::TextUnformatted(el.text.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                ImGui::TextUnformatted(el.text.c_str());
                ImGui::PopStyleColor();
            }
        } else if (el.type == "user") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;
            std::string name = resolve_user_ ? resolve_user_(el.user_id) : el.user_id;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::TextUnformatted(("@" + name).c_str());
            ImGui::PopStyleColor();
        } else if (el.type == "channel") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;
            std::string name = resolve_channel_ ? resolve_channel_(el.channel_id) : el.channel_id;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::TextUnformatted(("#" + name).c_str());
            ImGui::PopStyleColor();
        } else if (el.type == "usergroup") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;
            std::string name = resolve_usergroup_ ? resolve_usergroup_(el.usergroup_id)
                                                   : el.usergroup_id;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.3f, 1.0f));
            ImGui::TextUnformatted(("@" + name).c_str());
            ImGui::PopStyleColor();
        } else if (el.type == "link") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;
            std::string label = el.text.empty() ? el.url : el.text;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
            ImGui::TextUnformatted(label.c_str());
            ImGui::PopStyleColor();
        } else if (el.type == "emoji") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;
            ImGui::TextUnformatted((":" + el.text + ":").c_str());
        } else if (el.type == "broadcast") {
            if (!first) ImGui::SameLine(0, 0);
            first = false;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.3f, 1.0f));
            ImGui::TextUnformatted(("@" + el.text).c_str());
            ImGui::PopStyleColor();
        }
    }
}

void BlockRenderer::renderSection(const slack::Block& block, float wrap_width,
                                   const ImVec4& color) {
    if (block.text) {
        auto spans = parseMrkdwn(block.text->text);
        renderSpans(spans, wrap_width, color);
    }

    // fields in a 2-column layout
    if (!block.fields.empty()) {
        float col_width = wrap_width / 2.0f - 8.0f;
        for (size_t i = 0; i < block.fields.size(); i += 2) {
            auto spans1 = parseMrkdwn(block.fields[i].text);
            renderSpans(spans1, col_width, color);
            if (i + 1 < block.fields.size()) {
                ImGui::SameLine(col_width + 16.0f);
                auto spans2 = parseMrkdwn(block.fields[i + 1].text);
                renderSpans(spans2, col_width, color);
            }
        }
    }
}

void BlockRenderer::renderHeader(const slack::Block& block) {
    if (block.text) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        // headers are just bold, larger text. since we can't easily change font
        // size mid-frame with imgui, we just render bold-colored.
        ImGui::TextUnformatted(block.text->text.c_str());
        ImGui::PopStyleColor();
    }
}

void BlockRenderer::renderDivider(float width) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(pos.x, pos.y + 4),
        ImVec2(pos.x + width, pos.y + 4),
        IM_COL32(80, 80, 80, 200));
    ImGui::Dummy(ImVec2(0, 8));
}

void BlockRenderer::renderContext(const slack::Block& block, float wrap_width) {
    // context blocks are small metadata text + images in a row
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    bool first = true;
    for (auto& el : block.elements) {
        if (!first) ImGui::SameLine(0, 4);
        first = false;

        if (el.type == "mrkdwn" || el.type == "plain_text") {
            ImGui::TextUnformatted(el.text.c_str());
        } else if (el.type == "image" && image_renderer_) {
            // small inline image (16x16 or so)
            ImGui::TextUnformatted("[img]");
        }
    }
    ImGui::PopStyleColor();
}

void BlockRenderer::renderImage(const slack::Block& block, float max_width) {
    if (!block.title_text.empty()) {
        ImGui::TextUnformatted(block.title_text.c_str());
    }
    if (!block.alt_text.empty() && block.title_text.empty()) {
        ImGui::TextDisabled("%s", block.alt_text.c_str());
    }
    // actual image rendering is handled by BufferView's image pipeline
    // we just mark that there's an image block here
    if (!block.image_url.empty() && image_renderer_) {
        auto tex = image_renderer_->getTexture(block.image_url);
        if (tex.texture_id != 0) {
            float scale = std::min(max_width / static_cast<float>(tex.width), 1.0f);
            ImGui::Image((ImTextureID)(intptr_t)tex.texture_id,
                         ImVec2(tex.width * scale, tex.height * scale));
        } else {
            image_renderer_->renderPlaceholder("image", max_width, 100);
        }
    }
}

void BlockRenderer::renderActions(const slack::Block& block) {
    for (auto& el : block.elements) {
        if (el.type == "button") {
            if (ImGui::SmallButton(el.text.c_str())) {
                last_action_ = {true, el.action_id, block.block_id, el.value};
            }
            ImGui::SameLine(0, 4);
        }
    }
    ImGui::NewLine();
}

} // namespace conduit::render
