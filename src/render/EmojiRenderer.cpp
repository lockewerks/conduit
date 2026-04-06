#include "render/EmojiRenderer.h"
#include "render/EmojiMap.h"
#include "util/Logger.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <curl/curl.h>
#include "stb_image.h"

namespace conduit::render {

// codepoint map: shortcode name -> hex codepoint for Slack CDN URLs
// e.g. "thumbsup" -> "1f44d", "heart" -> "2764-fe0f"
static const std::unordered_map<std::string, std::string>& getCodepointMap() {
    static const std::unordered_map<std::string, std::string> map = {
        {"grinning", "1f600"}, {"smile", "1f604"}, {"laughing", "1f606"},
        {"joy", "1f602"}, {"rofl", "1f923"}, {"wink", "1f609"},
        {"blush", "1f60a"}, {"innocent", "1f607"}, {"heart_eyes", "1f60d"},
        {"kissing_heart", "1f618"}, {"yum", "1f60b"}, {"stuck_out_tongue", "1f61b"},
        {"stuck_out_tongue_winking_eye", "1f61c"}, {"sunglasses", "1f60e"},
        {"smirk", "1f60f"}, {"unamused", "1f612"}, {"disappointed", "1f61e"},
        {"worried", "1f61f"}, {"angry", "1f620"}, {"rage", "1f621"},
        {"cry", "1f622"}, {"sob", "1f62d"}, {"scream", "1f631"},
        {"confused", "1f615"}, {"hushed", "1f62f"}, {"sleeping", "1f634"},
        {"mask", "1f637"}, {"nerd_face", "1f913"}, {"thinking_face", "1f914"},
        {"face_with_rolling_eyes", "1f644"}, {"zipper_mouth_face", "1f910"},
        {"nauseated_face", "1f922"}, {"skull", "1f480"},
        {"thumbsup", "1f44d"}, {"+1", "1f44d"},
        {"thumbsdown", "1f44e"}, {"-1", "1f44e"},
        {"ok_hand", "1f44c"}, {"wave", "1f44b"}, {"clap", "1f44f"},
        {"raised_hands", "1f64c"}, {"pray", "1f64f"}, {"muscle", "1f4aa"},
        {"point_up", "261d-fe0f"}, {"point_down", "1f447"},
        {"point_left", "1f448"}, {"point_right", "1f449"},
        {"middle_finger", "1f595"}, {"v", "270c-fe0f"},
        {"metal", "1f918"}, {"call_me_hand", "1f919"},
        {"heart", "2764-fe0f"}, {"orange_heart", "1f9e1"},
        {"yellow_heart", "1f49b"}, {"green_heart", "1f49a"},
        {"blue_heart", "1f499"}, {"purple_heart", "1f49c"},
        {"broken_heart", "1f494"}, {"sparkling_heart", "1f496"},
        {"fire", "1f525"}, {"100", "1f4af"}, {"star", "2b50"},
        {"sparkles", "2728"}, {"boom", "1f4a5"}, {"zap", "26a1"},
        {"eyes", "1f440"}, {"eye", "1f441-fe0f"},
        {"tada", "1f389"}, {"confetti_ball", "1f38a"},
        {"rocket", "1f680"}, {"airplane", "2708-fe0f"},
        {"white_check_mark", "2705"}, {"x", "274c"},
        {"warning", "26a0-fe0f"}, {"no_entry", "26d4"},
        {"question", "2753"}, {"exclamation", "2757"},
        {"bulb", "1f4a1"}, {"memo", "1f4dd"}, {"pencil", "270f-fe0f"},
        {"calendar", "1f4c5"}, {"clock1", "1f550"},
        {"sun_with_face", "1f31e"}, {"rainbow", "1f308"},
        {"dog", "1f436"}, {"cat", "1f431"}, {"monkey_face", "1f435"},
        {"see_no_evil", "1f648"}, {"hear_no_evil", "1f649"},
        {"speak_no_evil", "1f64a"},
        {"pizza", "1f355"}, {"hamburger", "1f354"}, {"beer", "1f37a"},
        {"coffee", "2615"}, {"cake", "1f370"},
        {"trophy", "1f3c6"}, {"medal", "1f3c5"},
        {"shrug", "1f937"}, {"facepalm", "1f926"},
        {"rolling_on_the_floor_laughing", "1f923"},
        {"upside_down_face", "1f643"}, {"money_mouth_face", "1f911"},
        {"hugging_face", "1f917"}, {"sweat_smile", "1f605"},
        {"slightly_smiling_face", "1f642"}, {"neutral_face", "1f610"},
        {"expressionless", "1f611"}, {"no_mouth", "1f636"},
        {"relieved", "1f60c"}, {"pensive", "1f614"},
        {"sleepy", "1f62a"}, {"drooling_face", "1f924"},
        {"grimacing", "1f62c"}, {"persevere", "1f623"},
        {"triumph", "1f624"}, {"fearful", "1f628"},
        {"cold_sweat", "1f630"}, {"dizzy_face", "1f635"},
        {"exploding_head", "1f92f"}, {"cowboy_hat_face", "1f920"},
        {"partying_face", "1f973"}, {"hot_face", "1f975"},
        {"cold_face", "1f976"}, {"pleading_face", "1f97a"},
        {"saluting_face", "1fae1"},
        {"ghost", "1f47b"}, {"alien", "1f47d"}, {"robot_face", "1f916"},
        {"poop", "1f4a9"}, {"clown_face", "1f921"},
        {"flag-us", "1f1fa-1f1f8"}, {"flag-gb", "1f1ec-1f1e7"},
        {"heavy_plus_sign", "2795"}, {"heavy_minus_sign", "2796"},
        {"heavy_check_mark", "2714-fe0f"},
        {"arrow_right", "27a1-fe0f"}, {"arrow_left", "2b05-fe0f"},
        {"gem", "1f48e"}, {"crown", "1f451"},
        {"lock", "1f512"}, {"key", "1f511"},
        {"bell", "1f514"}, {"megaphone", "1f4e3"},
        {"speech_balloon", "1f4ac"}, {"thought_balloon", "1f4ad"},
        {"link", "1f517"}, {"wrench", "1f527"}, {"gear", "2699-fe0f"},
        {"package", "1f4e6"}, {"inbox_tray", "1f4e5"},
    };
    return map;
}

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* vec = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    vec->insert(vec->end(), (uint8_t*)contents, (uint8_t*)contents + total);
    return total;
}

EmojiRenderer::~EmojiRenderer() {
    for (auto& [_, tex] : textures_) {
        if (tex.texture_id) glDeleteTextures(1, &tex.texture_id);
    }
}

void EmojiRenderer::setCustomEmoji(const std::unordered_map<std::string, std::string>& name_to_url) {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_urls_ = name_to_url;
    LOG_INFO("emoji renderer: " + std::to_string(name_to_url.size()) + " custom emoji registered");
}

std::string EmojiRenderer::getStandardEmojiUrl(const std::string& name) const {
    auto& cmap = getCodepointMap();
    auto it = cmap.find(name);
    if (it == cmap.end()) return "";
    return "https://a.slack-edge.com/production-standard-emoji-assets/14.0/google-medium/"
           + it->second + ".png";
}

bool EmojiRenderer::renderInline(const std::string& emoji_name, float size) {
    if (size <= 0) size = ImGui::GetTextLineHeight();

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = textures_.find(emoji_name);

    if (it != textures_.end() && it->second.texture_id != 0) {
        // render the texture
        ImGui::Image((ImTextureID)(uintptr_t)it->second.texture_id, {size, size});
        return true;
    }

    if (it != textures_.end() && (it->second.loading || it->second.failed)) {
        return false; // still loading or permanently failed
    }

    // not loaded yet — kick off a download if we have a pool
    if (pool_) {
        // find the URL
        std::string url;
        auto custom_it = custom_urls_.find(emoji_name);
        if (custom_it != custom_urls_.end()) {
            url = custom_it->second;
        } else {
            url = getStandardEmojiUrl(emoji_name);
        }

        if (!url.empty() && in_flight_.find(emoji_name) == in_flight_.end()) {
            in_flight_.insert(emoji_name);
            textures_[emoji_name] = {0, 0, 0, true, false};

            std::string name = emoji_name;
            std::string cookie = cookie_;
            std::string token = auth_token_;
            pool_->enqueue([this, name, url, cookie, token]() {
                downloadEmoji(name, url);
            });
        }
    }

    return false;
}

void EmojiRenderer::downloadEmoji(const std::string& name, const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::lock_guard<std::mutex> lock(mutex_);
        textures_[name].failed = true;
        textures_[name].loading = false;
        in_flight_.erase(name);
        return;
    }

    std::vector<uint8_t> data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    // only add auth for slack URLs
    struct curl_slist* headers = nullptr;
    bool is_slack = (url.find("slack") != std::string::npos);
    if (is_slack && !auth_token_.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + auth_token_).c_str());
    }
    if (is_slack && !cookie_.empty()) {
        headers = curl_slist_append(headers, ("Cookie: d=" + cookie_).c_str());
    }
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || data.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        textures_[name].failed = true;
        textures_[name].loading = false;
        in_flight_.erase(name);
        return;
    }

    // decode the image
    int w, h, channels;
    uint8_t* pixels = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &channels, 4);
    if (!pixels) {
        std::lock_guard<std::mutex> lock(mutex_);
        textures_[name].failed = true;
        textures_[name].loading = false;
        in_flight_.erase(name);
        return;
    }

    std::vector<uint8_t> rgba(pixels, pixels + w * h * 4);
    stbi_image_free(pixels);

    // queue for GPU upload on main thread
    std::lock_guard<std::mutex> lock(mutex_);
    pending_uploads_.push_back({name, std::move(rgba), w, h});
    in_flight_.erase(name);
}

void EmojiRenderer::uploadPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& up : pending_uploads_) {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, up.width, up.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, up.rgba_data.data());

        textures_[up.name] = {tex, up.width, up.height, false, false};
    }
    pending_uploads_.clear();
}

void EmojiRenderer::prefetch(const std::string& emoji_name, const std::string& auth_token,
                              ThreadPool& pool) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (textures_.count(emoji_name) || in_flight_.count(emoji_name)) return;

    std::string url;
    auto custom_it = custom_urls_.find(emoji_name);
    if (custom_it != custom_urls_.end()) {
        url = custom_it->second;
    } else {
        url = getStandardEmojiUrl(emoji_name);
    }
    if (url.empty()) return;

    in_flight_.insert(emoji_name);
    textures_[emoji_name] = {0, 0, 0, true, false};

    std::string name = emoji_name;
    pool.enqueue([this, name, url]() {
        downloadEmoji(name, url);
    });
}

bool EmojiRenderer::hasEmoji(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (custom_urls_.count(name)) return true;
    return getCodepointMap().count(name) > 0;
}

std::vector<std::string> EmojiRenderer::allCustomNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (auto& [name, _] : custom_urls_) names.push_back(name);
    return names;
}

} // namespace conduit::render
