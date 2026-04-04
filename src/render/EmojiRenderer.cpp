#include "render/EmojiRenderer.h"
#include "util/Logger.h"

namespace conduit::render {

void EmojiRenderer::loadAtlas(const std::string& atlas_path) {
    // load the emoji spritesheet and build UV lookup
    // this would use stb_image to load the atlas and parse a companion JSON
    // for emoji positions. stubbed for now - we render text fallbacks instead.
    LOG_INFO("emoji atlas loading from " + atlas_path + " (stubbed)");
}

void EmojiRenderer::loadCustomEmoji(
    const std::unordered_map<std::string, std::string>& name_to_url) {
    // custom emoji are loaded lazily on first render
    // just record the URLs for now
    LOG_INFO("registered " + std::to_string(name_to_url.size()) + " custom emoji");
}

bool EmojiRenderer::renderInline(const std::string& emoji_name, float size) {
    // check custom textures first
    auto custom_it = custom_textures_.find(emoji_name);
    if (custom_it != custom_textures_.end() && custom_it->second != 0) {
        ImGui::Image(
            (ImTextureID)(uintptr_t)custom_it->second,
            {size, size});
        return true;
    }

    // check atlas
    auto atlas_it = atlas_uvs_.find(emoji_name);
    if (atlas_it != atlas_uvs_.end() && atlas_texture_ != 0) {
        ImVec2 uv0{atlas_it->second.x, atlas_it->second.y};
        ImVec2 uv1{atlas_it->second.z, atlas_it->second.w};
        ImGui::Image(
            (ImTextureID)(uintptr_t)atlas_texture_,
            {size, size}, uv0, uv1);
        return true;
    }

    // fallback: render the :name: as text
    // this is fine, most terminal-style clients do this anyway
    return false;
}

bool EmojiRenderer::hasEmoji(const std::string& name) const {
    return atlas_uvs_.count(name) > 0 || custom_textures_.count(name) > 0;
}

std::vector<std::string> EmojiRenderer::allNames() const {
    std::vector<std::string> names;
    for (auto& [name, _] : atlas_uvs_) names.push_back(name);
    for (auto& [name, _] : custom_textures_) names.push_back(name);
    return names;
}

} // namespace conduit::render
