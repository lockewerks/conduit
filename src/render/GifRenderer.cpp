#include "render/GifRenderer.h"
#include "util/Logger.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

namespace conduit::render {

GifRenderer::~GifRenderer() {
    for (auto& [_, gif] : gifs_) {
        for (auto tex : gif.frame_textures) {
            if (tex) glDeleteTextures(1, &tex);
        }
    }
}

AnimatedGif* GifRenderer::getGif(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gifs_.find(url);
    if (it != gifs_.end()) return &it->second;

    // create a placeholder
    AnimatedGif gif;
    gif.loading = true;
    gifs_[url] = gif;
    return &gifs_[url];
}

void GifRenderer::update(float delta_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, gif] : gifs_) {
        if (gif.loaded) {
            gif.update(delta_ms);
        }
    }
}

bool GifRenderer::renderInline(const std::string& url, float max_w, float max_h) {
    auto* gif = getGif(url);
    if (!gif || !gif->loaded || gif->frame_textures.empty()) {
        // placeholder
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.5f, 0.5f, 0.6f, 1.0f});
        ImGui::TextUnformatted("[loading gif...]");
        ImGui::PopStyleColor();
        return false;
    }

    GLuint tex = gif->currentTexture();
    float w = (float)gif->width;
    float h = (float)gif->height;
    if (w > max_w) { h *= max_w / w; w = max_w; }
    if (h > max_h) { w *= max_h / h; h = max_h; }

    ImGui::Image((ImTextureID)(uintptr_t)tex, {w, h});
    return true;
}

void GifRenderer::uploadPending() {
    // gif frame upload happens similarly to images
    // actual gif decoding with giflib would go here
    // for now this is a stub that'll be fleshed out when we need it
}

} // namespace conduit::render
