#include "render/ImageRenderer.h"
#include "util/Logger.h"

#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace conduit::render {

// curl callback for downloading binary data into a vector
static size_t imgWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* vec = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    vec->insert(vec->end(), (uint8_t*)contents, (uint8_t*)contents + total);
    return total;
}

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

// the whole point of this class: download an image, decode it, get it on the GPU
// all without blocking the main thread because we're not animals
void ImageRenderer::requestImage(const std::string& url, const std::string& auth_token,
                                  conduit::ThreadPool& pool) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // already have it or already fetching it? bail.
        if (textures_.count(url) && (textures_[url].texture_id || textures_[url].failed)) return;
        if (in_flight_.count(url)) return;
        in_flight_.insert(url);
        textures_[url] = TextureInfo{0, 0, 0, true, false};
    }

    pool.enqueue([this, url, auth_token]() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::lock_guard<std::mutex> lock(mutex_);
            textures_[url].loading = false;
            textures_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        std::vector<uint8_t> data;
        struct curl_slist* headers = nullptr;

        // only add auth header for slack URLs - external CDNs (tenor, giphy)
        // will reject or ignore our slack token
        bool is_slack = (url.find("slack.com") != std::string::npos ||
                         url.find("slack-edge.com") != std::string::npos);
        if (is_slack) {
            std::string auth = "Authorization: Bearer " + auth_token;
            headers = curl_slist_append(headers, auth.c_str());
            if (!cookie_.empty()) {
                std::string cookie_hdr = "Cookie: d=" + cookie_;
                headers = curl_slist_append(headers, cookie_hdr.c_str());
            }
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, imgWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Conduit/0.1");

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || data.empty()) {
            LOG_WARN("image download failed: " + url.substr(0, 60));
            std::lock_guard<std::mutex> lock(mutex_);
            textures_[url].loading = false;
            textures_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        // decode on this worker thread so we don't block rendering
        int w, h, channels;
        unsigned char* pixels = stbi_load_from_memory(data.data(), (int)data.size(),
                                                       &w, &h, &channels, 4);
        if (!pixels) {
            LOG_WARN("image decode failed: " + url.substr(0, 60));
            std::lock_guard<std::mutex> lock(mutex_);
            textures_[url].loading = false;
            textures_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        PendingUpload upload;
        upload.url = url;
        upload.width = w;
        upload.height = h;
        upload.data.assign(pixels, pixels + (w * h * 4));
        stbi_image_free(pixels);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_uploads_.push_back(std::move(upload));
            in_flight_.erase(url);
        }

        LOG_DEBUG("image decoded: " + std::to_string(w) + "x" + std::to_string(h));
    });

    evictOldest();
}

} // namespace conduit::render
