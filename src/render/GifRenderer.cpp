#include "render/GifRenderer.h"
#include "util/Logger.h"

#include <gif_lib.h>
#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

namespace conduit::render {

// curl doesn't care about your feelings, just your buffer
static size_t gifWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* vec = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    vec->insert(vec->end(), (uint8_t*)contents, (uint8_t*)contents + total);
    return total;
}

// giflib read function for decoding from memory because DGifOpenFileName
// doesn't know what a URL is (and frankly, neither do most people)
struct GifMemReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

static int gifReadFromMemory(GifFileType* gif, GifByteType* buf, int len) {
    auto* reader = static_cast<GifMemReader*>(gif->UserData);
    size_t avail = reader->size - reader->pos;
    size_t to_read = (size_t)len < avail ? (size_t)len : avail;
    memcpy(buf, reader->data + reader->pos, to_read);
    reader->pos += to_read;
    return (int)to_read;
}

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

    // create a placeholder so we don't spam requests
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
        // placeholder while the hamster wheel spins
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

void GifRenderer::requestGif(const std::string& url, const std::string& auth_token,
                              conduit::ThreadPool& pool) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // already loaded or already working on it? don't be that guy
        auto it = gifs_.find(url);
        if (it != gifs_.end() && (it->second.loaded || it->second.failed)) return;
        if (in_flight_.count(url)) return;
        in_flight_.insert(url);

        // make sure the placeholder exists
        if (it == gifs_.end()) {
            AnimatedGif gif;
            gif.loading = true;
            gifs_[url] = gif;
        }
    }

    pool.enqueue([this, url, auth_token]() {
        // step 1: download the raw bytes
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::lock_guard<std::mutex> lock(mutex_);
            gifs_[url].loading = false;
            gifs_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        std::vector<uint8_t> data;
        struct curl_slist* headers = nullptr;

        // only auth for slack URLs, not external CDNs
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
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Conduit/0.1");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gifWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || data.empty()) {
            LOG_WARN("gif download failed: " + url.substr(0, 60));
            std::lock_guard<std::mutex> lock(mutex_);
            gifs_[url].loading = false;
            gifs_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        // step 2: decode every frame with giflib
        int error = 0;
        GifMemReader reader{data.data(), data.size(), 0};
        GifFileType* gif = DGifOpen(&reader, gifReadFromMemory, &error);
        if (!gif) {
            LOG_WARN("giflib open failed: " + std::to_string(error));
            std::lock_guard<std::mutex> lock(mutex_);
            gifs_[url].loading = false;
            gifs_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        if (DGifSlurp(gif) != GIF_OK) {
            LOG_WARN("giflib slurp failed on: " + url.substr(0, 60));
            DGifCloseFile(gif, &error);
            std::lock_guard<std::mutex> lock(mutex_);
            gifs_[url].loading = false;
            gifs_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        int w = gif->SWidth;
        int h = gif->SHeight;
        int frame_count = gif->ImageCount;

        if (frame_count <= 0 || w <= 0 || h <= 0) {
            DGifCloseFile(gif, &error);
            std::lock_guard<std::mutex> lock(mutex_);
            gifs_[url].loading = false;
            gifs_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        // canvas that accumulates frames (GIF is a stateful format, yay)
        std::vector<uint8_t> canvas(w * h * 4, 0);

        PendingGifUpload upload;
        upload.url = url;
        upload.width = w;
        upload.height = h;

        for (int fi = 0; fi < frame_count; fi++) {
            SavedImage& si = gif->SavedImages[fi];
            GifImageDesc& desc = si.ImageDesc;

            // figure out the color map for this frame
            ColorMapObject* cmap = desc.ColorMap ? desc.ColorMap : gif->SColorMap;
            if (!cmap) continue;

            // parse the graphics control extension for delay and disposal
            int delay_ms = 100; // default 100ms if unspecified
            int disposal = 0;
            int transparent_idx = -1;

            for (int ei = 0; ei < si.ExtensionBlockCount; ei++) {
                ExtensionBlock& eb = si.ExtensionBlocks[ei];
                if (eb.Function == GRAPHICS_EXT_FUNC_CODE && eb.ByteCount >= 4) {
                    disposal = (eb.Bytes[0] >> 2) & 0x07;
                    if (eb.Bytes[0] & 0x01) {
                        transparent_idx = (unsigned char)eb.Bytes[3];
                    }
                    int raw_delay = ((unsigned char)eb.Bytes[2] << 8) | (unsigned char)eb.Bytes[1];
                    delay_ms = raw_delay * 10;
                    if (delay_ms <= 0) delay_ms = 100; // browsers treat 0 as ~100ms
                }
            }

            // save pre-frame canvas if disposal method is "restore to previous"
            std::vector<uint8_t> canvas_backup;
            if (disposal == 3) {
                canvas_backup = canvas;
            }

            // paint this frame's pixels onto the canvas
            for (int py = 0; py < desc.Height; py++) {
                for (int px = 0; px < desc.Width; px++) {
                    int src_idx = py * desc.Width + px;
                    if (src_idx >= si.ImageDesc.Width * si.ImageDesc.Height) continue;

                    int color_idx = si.RasterBits[src_idx];
                    if (color_idx == transparent_idx) continue;
                    if (color_idx >= cmap->ColorCount) continue;

                    GifColorType& c = cmap->Colors[color_idx];
                    int dst_x = desc.Left + px;
                    int dst_y = desc.Top + py;
                    if (dst_x >= w || dst_y >= h) continue;

                    int dst_idx = (dst_y * w + dst_x) * 4;
                    canvas[dst_idx + 0] = c.Red;
                    canvas[dst_idx + 1] = c.Green;
                    canvas[dst_idx + 2] = c.Blue;
                    canvas[dst_idx + 3] = 255;
                }
            }

            // snapshot the canvas as this frame's RGBA data
            upload.frames.push_back(canvas);
            upload.delays_ms.push_back(delay_ms);

            // apply disposal method for the next frame
            if (disposal == 2) {
                // restore to background: clear the frame region
                for (int py = 0; py < desc.Height; py++) {
                    for (int px = 0; px < desc.Width; px++) {
                        int dst_x = desc.Left + px;
                        int dst_y = desc.Top + py;
                        if (dst_x >= w || dst_y >= h) continue;
                        int dst_idx = (dst_y * w + dst_x) * 4;
                        canvas[dst_idx + 0] = 0;
                        canvas[dst_idx + 1] = 0;
                        canvas[dst_idx + 2] = 0;
                        canvas[dst_idx + 3] = 0;
                    }
                }
            } else if (disposal == 3) {
                // restore to previous: revert the canvas
                canvas = canvas_backup;
            }
            // disposal 0 and 1: leave the canvas as-is (which is what we want)
        }

        DGifCloseFile(gif, &error);

        if (upload.frames.empty()) {
            LOG_WARN("gif had zero usable frames: " + url.substr(0, 60));
            std::lock_guard<std::mutex> lock(mutex_);
            gifs_[url].loading = false;
            gifs_[url].failed = true;
            in_flight_.erase(url);
            return;
        }

        LOG_DEBUG("gif decoded: " + std::to_string(w) + "x" + std::to_string(h) +
                  " (" + std::to_string(upload.frames.size()) + " frames)");

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_gif_uploads_.push_back(std::move(upload));
            in_flight_.erase(url);
        }
    });
}

void GifRenderer::uploadPending() {
    std::lock_guard<std::mutex> lock(mutex_);

    // upload 2-3 frames worth of GIF data per app frame to keep things smooth.
    // we process one PendingGifUpload at a time since each can have many frames.
    int textures_created = 0;
    const int max_per_frame = 3;

    while (!pending_gif_uploads_.empty() && textures_created < max_per_frame) {
        auto& pending = pending_gif_uploads_.back();

        auto it = gifs_.find(pending.url);
        if (it == gifs_.end()) {
            // the entry vanished somehow, toss it
            pending_gif_uploads_.pop_back();
            continue;
        }

        AnimatedGif& gif = it->second;

        // upload frames in batches so we don't nuke the frame budget
        size_t already_uploaded = gif.frame_textures.size();
        size_t total_frames = pending.frames.size();
        size_t batch_end = std::min(already_uploaded + max_per_frame - textures_created,
                                     total_frames);

        for (size_t i = already_uploaded; i < batch_end; i++) {
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         pending.width, pending.height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pending.frames[i].data());

            gif.frame_textures.push_back(tex);
            gif.frame_delays_ms.push_back(pending.delays_ms[i]);
            textures_created++;
        }

        // did we finish all frames for this GIF?
        if (gif.frame_textures.size() >= total_frames) {
            gif.width = pending.width;
            gif.height = pending.height;
            gif.loading = false;
            gif.loaded = true;
            pending_gif_uploads_.pop_back();
        } else {
            // more frames to go, pick it up next time
            break;
        }
    }
}

} // namespace conduit::render
