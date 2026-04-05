#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <imgui.h>
#include "util/ThreadPool.h"

// forward declare so we don't include GL everywhere
typedef unsigned int GLuint;

namespace conduit::render {

struct TextureInfo {
    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
    bool loading = false;
    bool failed = false;
};

// handles image loading, GPU texture upload, and LRU cache management
// images are decoded on worker threads but uploaded on main thread (GL requirement)
class ImageRenderer {
public:
    ImageRenderer(float max_width = 360.0f, float max_height = 240.0f);
    ~ImageRenderer();

    // request an image for rendering. returns info if ready, starts loading if not.
    TextureInfo getTexture(const std::string& url);

    // call this on the main thread to upload any pending decoded images to the GPU
    void uploadPending();

    // render an image at the current imgui cursor position
    // returns true if the image was rendered, false if still loading
    bool renderInline(const std::string& url);

    // render a placeholder while loading
    void renderPlaceholder(const std::string& filename, float width, float height);

    // kick off an async download + decode for an image URL
    void requestImage(const std::string& url, const std::string& auth_token,
                      conduit::ThreadPool& pool);

    // LRU management
    void setMaxTextures(int max) { max_textures_ = max; }
    void evictOldest();

    float maxWidth() const { return max_width_; }
    float maxHeight() const { return max_height_; }

private:
    float max_width_;
    float max_height_;
    int max_textures_ = 200;

    std::mutex mutex_;
    std::unordered_map<std::string, TextureInfo> textures_;

    // pending uploads: decoded image data waiting for GL upload on main thread
    struct PendingUpload {
        std::string url;
        std::vector<uint8_t> data;
        int width, height;
    };
    std::vector<PendingUpload> pending_uploads_;

    // URLs currently being downloaded so we don't double-fetch
    std::unordered_set<std::string> in_flight_;

    // LRU tracking
    std::vector<std::string> access_order_;
    void touch(const std::string& url);

    ImVec2 scaledSize(int orig_w, int orig_h) const;
};

} // namespace conduit::render
