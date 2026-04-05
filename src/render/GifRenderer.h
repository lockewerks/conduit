#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <imgui.h>
#include "util/ThreadPool.h"

typedef unsigned int GLuint;

namespace conduit::render {

struct AnimatedGif {
    std::vector<GLuint> frame_textures;
    std::vector<int> frame_delays_ms;
    int current_frame = 0;
    float elapsed_ms = 0.0f;
    int width = 0, height = 0;
    bool loaded = false;
    bool loading = false;
    bool failed = false;

    GLuint currentTexture() const {
        if (frame_textures.empty()) return 0;
        return frame_textures[current_frame];
    }

    void update(float delta_ms) {
        if (frame_textures.empty() || frame_delays_ms.empty()) return;
        elapsed_ms += delta_ms;
        while (elapsed_ms >= frame_delays_ms[current_frame]) {
            elapsed_ms -= frame_delays_ms[current_frame];
            current_frame = (current_frame + 1) % (int)frame_textures.size();
        }
    }
};

// handles animated GIF decoding, frame management, and rendering
// frames are decoded on worker threads, uploaded to GPU on main thread
class GifRenderer {
public:
    ~GifRenderer();

    AnimatedGif* getGif(const std::string& url);
    void update(float delta_ms);
    bool renderInline(const std::string& url, float max_w, float max_h);

    // upload pending decoded frames on the main thread
    void uploadPending();

    void setCookie(const std::string& cookie) { cookie_ = cookie; }

    // kick off async download + decode for a GIF URL
    void requestGif(const std::string& url, const std::string& auth_token,
                    conduit::ThreadPool& pool);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, AnimatedGif> gifs_;

    // decoded frame data waiting for GL upload on the main thread
    struct PendingGifUpload {
        std::string url;
        std::vector<std::vector<uint8_t>> frames; // RGBA pixel data per frame
        std::vector<int> delays_ms;
        int width = 0;
        int height = 0;
    };
    std::vector<PendingGifUpload> pending_gif_uploads_;

    std::string cookie_;

    // URLs currently being fetched so we don't fire off duplicate requests
    std::unordered_set<std::string> in_flight_;
};

} // namespace conduit::render
