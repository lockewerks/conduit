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

    // use the FOREGROUND draw list so we render on top of everything,
    // not buried under inline images in the chat buffer
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // dim the entire screen
    dl->AddRectFilled({0, 0}, {screen_w, screen_h},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.75f}));

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
        return;
    }

    if (texture_.texture_id == 0 || texture_.width == 0 || texture_.height == 0) {
        ImVec2 text_size = ImGui::CalcTextSize("Loading...");
        dl->AddText({(screen_w - text_size.x) * 0.5f, (screen_h - text_size.y) * 0.5f},
                    ImGui::ColorConvertFloat4ToU32(theme.text_dim), "Loading...");
        if (ImGui::IsMouseClicked(0)) close();
        return;
    }

    float img_w, img_h;
    fitToScreen(screen_w, screen_h, img_w, img_h);

    float img_x = (screen_w - img_w) * 0.5f;
    float img_y = (screen_h - img_h) * 0.5f;

    // subtle shadow behind the image so it pops
    dl->AddRectFilled({img_x - 2, img_y - 2}, {img_x + img_w + 2, img_y + img_h + 2},
                       ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 0.5f}));

    dl->AddImage((ImTextureID)(intptr_t)texture_.texture_id,
                 {img_x, img_y}, {img_x + img_w, img_y + img_h});

    // close hint
    std::string hint = "ESC or click to close";
    ImVec2 hint_size = ImGui::CalcTextSize(hint.c_str());
    dl->AddText({screen_w - hint_size.x - 16.0f, 12.0f},
                ImGui::ColorConvertFloat4ToU32(theme.text_dim), hint.c_str());

    // click outside the image to close
    if (ImGui::IsMouseClicked(0)) {
        ImVec2 mouse = ImGui::GetMousePos();
        if (mouse.x < img_x || mouse.x > img_x + img_w ||
            mouse.y < img_y || mouse.y > img_y + img_h) {
            close();
        }
    }
}

void FilePreview::fitToScreen(float screen_w, float screen_h, float& out_w, float& out_h) const {
    float max_w = screen_w * 0.85f;
    float max_h = screen_h * 0.80f;

    float tex_w = static_cast<float>(texture_.width);
    float tex_h = static_cast<float>(texture_.height);

    float scale = std::min(max_w / tex_w, max_h / tex_h);
    scale = std::min(scale, 2.0f);

    out_w = tex_w * scale;
    out_h = tex_h * scale;
}

} // namespace conduit::ui
