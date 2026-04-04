#include "render/CodeBlock.h"
#include <sstream>

namespace conduit::render {

void CodeBlock::render(const std::string& code, float width) {
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // dark background for the code block
    ImVec2 text_size = ImGui::CalcTextSize(code.c_str(), nullptr, false, width - 16);
    ImGui::GetWindowDrawList()->AddRectFilled(
        {pos.x, pos.y},
        {pos.x + width, pos.y + text_size.y + 12},
        ImGui::ColorConvertFloat4ToU32({0.06f, 0.06f, 0.09f, 1.0f}),
        2.0f);

    // left accent border
    ImGui::GetWindowDrawList()->AddRectFilled(
        {pos.x, pos.y},
        {pos.x + 3, pos.y + text_size.y + 12},
        ImGui::ColorConvertFloat4ToU32({0.3f, 0.5f, 0.8f, 0.6f}));

    ImGui::SetCursorScreenPos({pos.x + 10, pos.y + 6});

    // render with basic syntax highlighting
    // we split by lines and words and colorize known keywords
    std::istringstream stream(code);
    std::string line;
    bool first_line = true;

    while (std::getline(stream, line)) {
        if (!first_line) {
            ImGui::SetCursorPosX(pos.x + 10 - ImGui::GetWindowPos().x);
        }
        first_line = false;

        // tokenize and colorize
        std::string word;
        bool in_string = false;
        bool in_comment = false;
        char string_char = 0;

        for (size_t i = 0; i < line.size(); i++) {
            char c = line[i];

            if (in_comment) {
                word += c;
                continue;
            }

            // string detection
            if ((c == '"' || c == '\'') && !in_string) {
                if (!word.empty()) {
                    ImVec4 color = {0.85f, 0.85f, 0.8f, 1.0f};
                    if (isKeyword(word)) color = {0.78f, 0.58f, 1.0f, 1.0f};
                    else if (isType(word)) color = {0.47f, 0.78f, 1.0f, 1.0f};
                    else if (isLiteral(word)) color = {1.0f, 0.65f, 0.3f, 1.0f};
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(word.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 0);
                    word.clear();
                }
                in_string = true;
                string_char = c;
                word += c;
                continue;
            }

            if (in_string && c == string_char) {
                word += c;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.53f, 0.8f, 0.53f, 1.0f});
                ImGui::TextUnformatted(word.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 0);
                word.clear();
                in_string = false;
                continue;
            }

            if (in_string) {
                word += c;
                continue;
            }

            // line comment
            if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                if (!word.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.85f, 0.85f, 0.8f, 1.0f});
                    ImGui::TextUnformatted(word.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 0);
                    word.clear();
                }
                word = line.substr(i);
                in_comment = true;
                continue;
            }

            // word boundary
            if (!std::isalnum(c) && c != '_') {
                if (!word.empty()) {
                    ImVec4 color = {0.85f, 0.85f, 0.8f, 1.0f};
                    if (isKeyword(word)) color = {0.78f, 0.58f, 1.0f, 1.0f};
                    else if (isType(word)) color = {0.47f, 0.78f, 1.0f, 1.0f};
                    else if (isLiteral(word)) color = {1.0f, 0.65f, 0.3f, 1.0f};
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(word.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 0);
                    word.clear();
                }
                // render the punctuation
                char buf[2] = {c, 0};
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.7f, 0.7f, 0.7f, 1.0f});
                ImGui::TextUnformatted(buf);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 0);
            } else {
                word += c;
            }
        }

        // flush remaining word
        if (!word.empty()) {
            ImVec4 color = in_comment ? ImVec4{0.45f, 0.55f, 0.45f, 1.0f}
                                      : ImVec4{0.85f, 0.85f, 0.8f, 1.0f};
            if (!in_comment) {
                if (isKeyword(word)) color = {0.78f, 0.58f, 1.0f, 1.0f};
                else if (isType(word)) color = {0.47f, 0.78f, 1.0f, 1.0f};
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(word.c_str());
            ImGui::PopStyleColor();
        }

        // ImGui doesn't auto-newline after SameLine, so we force it
        ImGui::NewLine();
    }

    // add some bottom padding
    ImGui::SetCursorScreenPos({pos.x, pos.y + text_size.y + 14});
}

bool CodeBlock::isKeyword(const std::string& w) {
    // common keywords across popular languages
    static const char* keywords[] = {
        "if", "else", "for", "while", "do", "switch", "case", "break", "continue",
        "return", "try", "catch", "throw", "class", "struct", "enum", "namespace",
        "public", "private", "protected", "virtual", "override", "const", "static",
        "auto", "using", "typedef", "template", "typename", "inline", "extern",
        "import", "export", "from", "def", "fn", "func", "function", "let", "var",
        "const", "async", "await", "yield", "new", "delete", "this", "self", "super",
        nullptr
    };
    for (int i = 0; keywords[i]; i++) {
        if (w == keywords[i]) return true;
    }
    return false;
}

bool CodeBlock::isType(const std::string& w) {
    static const char* types[] = {
        "int", "float", "double", "char", "bool", "void", "string", "String",
        "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "vector", "map", "set", "list", "array", "optional", "pair", "tuple",
        nullptr
    };
    for (int i = 0; types[i]; i++) {
        if (w == types[i]) return true;
    }
    return false;
}

bool CodeBlock::isLiteral(const std::string& w) {
    if (w == "true" || w == "false" || w == "nullptr" || w == "null" ||
        w == "None" || w == "nil" || w == "undefined") return true;
    // check if it's a number
    if (!w.empty() && (std::isdigit(w[0]) || (w[0] == '-' && w.size() > 1))) return true;
    return false;
}

} // namespace conduit::render
