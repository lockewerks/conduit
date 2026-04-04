#pragma once
#include <string>
#include <imgui.h>

namespace conduit::render {

// renders syntax-highlighted code blocks
// "highlighted" is generous - we do keyword coloring, not tree-sitter level stuff
class CodeBlock {
public:
    static void render(const std::string& code, float width);

private:
    // basic keyword detection for common languages
    static bool isKeyword(const std::string& word);
    static bool isType(const std::string& word);
    static bool isLiteral(const std::string& word);
};

} // namespace conduit::render
