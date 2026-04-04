#pragma once
#include <string>
#include <unordered_map>
#include <imgui.h>

typedef unsigned int GLuint;

namespace conduit::render {

// renders emoji inline with text
// supports both standard emoji (from a spritesheet atlas) and custom workspace emoji
class EmojiRenderer {
public:
    void loadAtlas(const std::string& atlas_path);
    void loadCustomEmoji(const std::unordered_map<std::string, std::string>& name_to_url);

    // render an emoji at the current cursor position
    // returns true if the emoji was found and rendered
    bool renderInline(const std::string& emoji_name, float size = 16.0f);

    // check if we have this emoji
    bool hasEmoji(const std::string& name) const;

    // get all known emoji names (for tab completion)
    std::vector<std::string> allNames() const;

private:
    GLuint atlas_texture_ = 0;
    std::unordered_map<std::string, ImVec4> atlas_uvs_; // name -> (u0,v0,u1,v1)
    std::unordered_map<std::string, GLuint> custom_textures_;
};

} // namespace conduit::render
