#include "render/AttachmentRenderer.h"
#include "render/TextRenderer.h"

namespace conduit::render {

float AttachmentRenderer::render(const std::vector<slack::Attachment>& attachments,
                                  float wrap_width, const ImVec4& default_color) {
    float start_y = ImGui::GetCursorPosY();

    for (auto& att : attachments) {
        // skip attachments that are just images (those get promoted to files already)
        if (att.title.empty() && att.text.empty() && att.fallback.empty()) continue;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        auto* dl = ImGui::GetWindowDrawList();

        // colored left border
        ImU32 border_color = att.color.empty() ? IM_COL32(100, 100, 100, 200)
                                                : parseColor(att.color);

        float card_start_y = pos.y;
        float indent = 12.0f;
        ImGui::Indent(indent);

        // title
        if (!att.title.empty()) {
            if (!att.title_link.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::TextWrapped("%s", att.title.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ImGui::TextWrapped("%s", att.title.c_str());
                ImGui::PopStyleColor();
            }
        }

        // text body
        if (!att.text.empty()) {
            auto spans = parseMrkdwn(att.text);
            renderSpans(spans, wrap_width - indent - 4, default_color);
        } else if (!att.fallback.empty() && att.title.empty()) {
            ImGui::TextDisabled("%s", att.fallback.c_str());
        }

        // inline image
        if (!att.image_url.empty() && image_renderer_) {
            auto tex = image_renderer_->getTexture(att.image_url);
            if (tex.texture_id != 0) {
                float max_w = wrap_width - indent - 4;
                float scale = std::min(max_w / static_cast<float>(tex.width), 1.0f);
                ImGui::Image((ImTextureID)(intptr_t)tex.texture_id,
                             ImVec2(tex.width * scale, tex.height * scale));
            } else {
                image_renderer_->renderPlaceholder("image", wrap_width - indent, 60);
            }
        }

        float card_end_y = ImGui::GetCursorScreenPos().y;
        ImGui::Unindent(indent);

        // draw the colored left border
        dl->AddRectFilled(
            ImVec2(pos.x, card_start_y),
            ImVec2(pos.x + 3, card_end_y),
            border_color);

        ImGui::Dummy(ImVec2(0, 4));
    }

    return ImGui::GetCursorPosY() - start_y;
}

ImU32 AttachmentRenderer::parseColor(const std::string& hex) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);

    if (h.size() == 6) {
        unsigned int r = 0, g = 0, b = 0;
        try {
            r = std::stoul(h.substr(0, 2), nullptr, 16);
            g = std::stoul(h.substr(2, 2), nullptr, 16);
            b = std::stoul(h.substr(4, 2), nullptr, 16);
        } catch (...) {
            return IM_COL32(100, 100, 100, 200);
        }
        return IM_COL32(r, g, b, 255);
    }

    // slack also uses named colors
    if (h == "good") return IM_COL32(46, 182, 125, 255);
    if (h == "warning") return IM_COL32(224, 174, 50, 255);
    if (h == "danger") return IM_COL32(224, 64, 55, 255);

    return IM_COL32(100, 100, 100, 200);
}

} // namespace conduit::render
