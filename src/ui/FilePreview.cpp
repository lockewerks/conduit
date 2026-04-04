#include "ui/FilePreview.h"
#include <imgui.h>
#include <algorithm>

namespace conduit::ui {

void FilePreview::open(const std::string& url, const TextureInfo& tex) {
    is_open_ = true;
    url_ = url;
    texture_ = tex;
}

void FilePreview::close() {
    is_open_ = false;
}

void FilePreview::render(float screen_w, float screen_h, const Theme& theme) {
    if (!is_open_) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // dim the background so the user knows they're in a modal
    dl->AddRectFilled({0, 0}, {screen_w, screen_h},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.7f}));

    // escape to close, obviously
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
        return;
    }

    if (texture_.texture_id == 0 || texture_.width == 0 || texture_.height == 0) {
        // no texture loaded yet - show a placeholder instead of a blank void
        ImVec2 text_size = ImGui::CalcTextSize("Loading...");
        float cx = (screen_w - text_size.x) * 0.5f;
        float cy = (screen_h - text_size.y) * 0.5f;
        dl->AddText({cx, cy}, ImGui::ColorConvertFloat4ToU32(theme.text_dim), "Loading...");
        return;
    }

    float img_w, img_h;
    fitToScreen(screen_w, screen_h, img_w, img_h);

    float img_x = (screen_w - img_w) * 0.5f;
    float img_y = (screen_h - img_h) * 0.5f;

    // the actual image
    dl->AddImage((ImTextureID)(intptr_t)texture_.texture_id,
                 {img_x, img_y}, {img_x + img_w, img_y + img_h});

    // filename/url at the bottom so people know what they're looking at
    ImVec2 url_size = ImGui::CalcTextSize(url_.c_str());
    float url_x = (screen_w - url_size.x) * 0.5f;
    float url_y = img_y + img_h + 12.0f;
    dl->AddText({url_x, url_y}, ImGui::ColorConvertFloat4ToU32(theme.text_dim), url_.c_str());
}

void FilePreview::fitToScreen(float screen_w, float screen_h, float& out_w, float& out_h) const {
    // leave some breathing room around the edges
    float max_w = screen_w * 0.85f;
    float max_h = screen_h * 0.80f;

    float tex_w = static_cast<float>(texture_.width);
    float tex_h = static_cast<float>(texture_.height);

    float scale = std::min(max_w / tex_w, max_h / tex_h);

    // don't upscale tiny images to fill the screen, that looks terrible
    scale = std::min(scale, 1.0f);

    out_w = tex_w * scale;
    out_h = tex_h * scale;
}

} // namespace conduit::ui
