#include "render/ImageRenderer.h"
#include "util/Logger.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace conduit::render {

ImageRenderer::ImageRenderer(float max_width, float max_height)
    : max_width_(max_width), max_height_(max_height) {}

ImageRenderer::~ImageRenderer() {
    // clean up GL textures
    for (auto& [_, info] : textures_) {
        if (info.texture_id) {
            glDeleteTextures(1, &info.texture_id);
        }
    }
}

TextureInfo ImageRenderer::getTexture(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = textures_.find(url);
    if (it != textures_.end()) {
        touch(url);
        return it->second;
    }

    // not loaded yet, create a placeholder entry
    TextureInfo info;
    info.loading = true;
    textures_[url] = info;
    return info;
}

void ImageRenderer::uploadPending() {
    std::lock_guard<std::mutex> lock(mutex_);

    // only upload a couple per frame to avoid stutter
    int uploads_this_frame = 0;
    while (!pending_uploads_.empty() && uploads_this_frame < 2) {
        auto& pending = pending_uploads_.back();

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pending.width, pending.height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, pending.data.data());

        auto it = textures_.find(pending.url);
        if (it != textures_.end()) {
            it->second.texture_id = tex;
            it->second.width = pending.width;
            it->second.height = pending.height;
            it->second.loading = false;
        }

        pending_uploads_.pop_back();
        uploads_this_frame++;
    }
}

bool ImageRenderer::renderInline(const std::string& url) {
    auto info = getTexture(url);

    if (info.loading) {
        renderPlaceholder("[loading...]", max_width_ * 0.5f, 20.0f);
        return false;
    }

    if (info.failed || !info.texture_id) {
        renderPlaceholder("[image failed]", max_width_ * 0.5f, 20.0f);
        return false;
    }

    ImVec2 size = scaledSize(info.width, info.height);
    ImGui::Image((ImTextureID)(uintptr_t)info.texture_id, size);
    return true;
}

void ImageRenderer::renderPlaceholder(const std::string& filename, float width, float height) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, {pos.x + width, pos.y + height},
        ImGui::ColorConvertFloat4ToU32({0.15f, 0.15f, 0.2f, 1.0f}));
    ImGui::SetCursorScreenPos({pos.x + 4, pos.y + 2});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.5f, 0.5f, 0.6f, 1.0f});
    ImGui::TextUnformatted(filename.c_str());
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos({pos.x, pos.y + height + 2});
}

void ImageRenderer::evictOldest() {
    if ((int)textures_.size() <= max_textures_) return;

    // evict from the front of the access order (least recently used)
    while ((int)textures_.size() > max_textures_ && !access_order_.empty()) {
        auto& url = access_order_.front();
        auto it = textures_.find(url);
        if (it != textures_.end()) {
            if (it->second.texture_id) {
                glDeleteTextures(1, &it->second.texture_id);
            }
            textures_.erase(it);
        }
        access_order_.erase(access_order_.begin());
    }
}

void ImageRenderer::touch(const std::string& url) {
    // move to end of access order
    auto it = std::find(access_order_.begin(), access_order_.end(), url);
    if (it != access_order_.end()) {
        access_order_.erase(it);
    }
    access_order_.push_back(url);
}

ImVec2 ImageRenderer::scaledSize(int orig_w, int orig_h) const {
    float w = static_cast<float>(orig_w);
    float h = static_cast<float>(orig_h);

    if (w > max_width_) {
        float scale = max_width_ / w;
        w = max_width_;
        h *= scale;
    }
    if (h > max_height_) {
        float scale = max_height_ / h;
        h = max_height_;
        w *= scale;
    }

    return {w, h};
}

} // namespace conduit::render
