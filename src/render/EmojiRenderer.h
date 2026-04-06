#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <imgui.h>
#include "util/ThreadPool.h"

typedef unsigned int GLuint;

namespace conduit::render {

// renders emoji as inline textures instead of font glyphs.
// supports both standard emoji (from Slack's CDN) and custom workspace emoji
// (including animated GIFs). downloads happen on worker threads, GPU uploads
// on main thread, same pattern as ImageRenderer.
class EmojiRenderer {
public:
    ~EmojiRenderer();

    // set custom workspace emoji (name -> URL from emoji.list API)
    void setCustomEmoji(const std::unordered_map<std::string, std::string>& name_to_url);

    // render an emoji inline at the current cursor position
    // returns true if rendered, false if not available (caller should fall back to text)
    bool renderInline(const std::string& emoji_name, float size = 0.0f);

    // call on main thread each frame to upload decoded images to GPU
    void uploadPending();

    // kick off background downloads for emoji we'll need soon
    void prefetch(const std::string& emoji_name, const std::string& auth_token,
                  ThreadPool& pool);

    void setCookie(const std::string& cookie) { cookie_ = cookie; }
    void setAuthToken(const std::string& token) { auth_token_ = token; }
    void setThreadPool(ThreadPool* pool) { pool_ = pool; }

    // check if we have this emoji loaded (or loading)
    bool hasEmoji(const std::string& name) const;

    // get all known emoji names
    std::vector<std::string> allCustomNames() const;

private:
    struct EmojiTexture {
        GLuint texture_id = 0;
        int width = 0;
        int height = 0;
        bool loading = false;
        bool failed = false;
    };

    struct PendingUpload {
        std::string name;
        std::vector<uint8_t> rgba_data;
        int width, height;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, EmojiTexture> textures_;
    std::unordered_map<std::string, std::string> custom_urls_; // name -> URL
    std::vector<PendingUpload> pending_uploads_;
    std::unordered_set<std::string> in_flight_;

    std::string cookie_;
    std::string auth_token_;
    ThreadPool* pool_ = nullptr;

    // get the Slack CDN URL for a standard emoji by shortcode name
    std::string getStandardEmojiUrl(const std::string& name) const;

    // download and decode an emoji image (runs on worker thread)
    void downloadEmoji(const std::string& name, const std::string& url);
};

} // namespace conduit::render
