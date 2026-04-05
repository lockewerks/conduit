#include "render/TextRenderer.h"
#include <algorithm>

namespace conduit::render {

// slack mrkdwn parser, take two.
// the first version had issues with multi-line bold, emoji-inside-bold,
// and generally being too naive about where formatting markers can appear.
// this version handles newlines properly and doesn't eat colons.

static bool isWordBoundary(const std::string& text, size_t pos) {
    if (pos == 0 || pos >= text.size()) return true;
    char c = text[pos - 1];
    return c == ' ' || c == '\n' || c == '\t' || c == '(' || c == '[' || c == '{';
}

static bool isEndBoundary(const std::string& text, size_t pos) {
    if (pos + 1 >= text.size()) return true;
    char c = text[pos + 1];
    return c == ' ' || c == '\n' || c == '\t' || c == '.' || c == ',' ||
           c == '!' || c == '?' || c == ')' || c == ']' || c == '}' || c == ':';
}

// find closing marker on the SAME line (slack doesn't allow cross-line formatting)
static size_t findClosingMarker(const std::string& text, size_t start, char marker) {
    for (size_t i = start; i < text.size(); i++) {
        if (text[i] == '\n') return std::string::npos; // hit newline, no dice
        if (text[i] == marker && i > start) return i;
    }
    return std::string::npos;
}

std::vector<TextSpan> parseMrkdwn(const std::string& text) {
    std::vector<TextSpan> spans;
    if (text.empty()) return spans;

    std::string current;
    size_t i = 0;

    auto flushCurrent = [&]() {
        if (!current.empty()) {
            spans.push_back({TextSpan::Style::Normal, current});
            current.clear();
        }
    };

    while (i < text.size()) {
        char c = text[i];

        // newlines become their own spans so the renderer knows to break
        if (c == '\n') {
            flushCurrent();
            spans.push_back({TextSpan::Style::Normal, "\n"});
            i++;
            continue;
        }

        // code blocks (``` ... ```) - these CAN span newlines
        if (c == '`' && i + 2 < text.size() && text[i + 1] == '`' && text[i + 2] == '`') {
            flushCurrent();
            size_t end = text.find("```", i + 3);
            if (end != std::string::npos) {
                std::string code = text.substr(i + 3, end - (i + 3));
                if (!code.empty() && code[0] == '\n') code = code.substr(1);
                if (!code.empty() && code.back() == '\n') code.pop_back();
                spans.push_back({TextSpan::Style::CodeBlock, code});
                i = end + 3;
            } else {
                current += "```";
                i += 3;
            }
            continue;
        }

        // inline code (`) - single line only
        if (c == '`') {
            size_t end = findClosingMarker(text, i + 1, '`');
            if (end != std::string::npos) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Code, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
            } else {
                current += c;
                i++;
            }
            continue;
        }

        // slack-style links and refs: <...>
        if (c == '<') {
            size_t end = text.find('>', i + 1);
            if (end != std::string::npos) {
                flushCurrent();
                std::string inner = text.substr(i + 1, end - (i + 1));

                if (inner.starts_with("@U") || inner.starts_with("@W")) {
                    auto pipe = inner.find('|');
                    std::string uid = (pipe != std::string::npos) ? inner.substr(1, pipe - 1) : inner.substr(1);
                    std::string display = (pipe != std::string::npos) ? "@" + inner.substr(pipe + 1) : "@" + uid;
                    TextSpan span{TextSpan::Style::Mention, display};
                    span.reference = uid;
                    span.color = {0.47f, 0.78f, 1.0f, 1.0f};
                    spans.push_back(span);
                } else if (inner.starts_with("#C")) {
                    auto pipe = inner.find('|');
                    std::string cid = (pipe != std::string::npos) ? inner.substr(1, pipe - 1) : inner.substr(1);
                    std::string name = (pipe != std::string::npos) ? "#" + inner.substr(pipe + 1) : "#" + cid;
                    TextSpan span{TextSpan::Style::ChannelRef, name};
                    span.reference = cid;
                    span.color = {0.47f, 0.78f, 1.0f, 1.0f};
                    spans.push_back(span);
                } else if (inner.starts_with("!")) {
                    std::string broadcast = inner.substr(1);
                    auto pipe = broadcast.find('|');
                    if (pipe != std::string::npos) broadcast = broadcast.substr(0, pipe);
                    TextSpan span{TextSpan::Style::BroadcastMention, "@" + broadcast};
                    span.color = {1.0f, 0.85f, 0.4f, 1.0f};
                    spans.push_back(span);
                } else {
                    auto pipe = inner.find('|');
                    std::string url = (pipe != std::string::npos) ? inner.substr(0, pipe) : inner;
                    std::string label = (pipe != std::string::npos) ? inner.substr(pipe + 1) : url;
                    TextSpan span{TextSpan::Style::Link, label};
                    span.link_url = url;
                    span.color = {0.4f, 0.6f, 1.0f, 1.0f};
                    spans.push_back(span);
                }
                i = end + 1;
                continue;
            }
        }

        // emoji :name: - validated more carefully
        if (c == ':' && i + 2 < text.size()) {
            size_t end = text.find(':', i + 1);
            if (end != std::string::npos && end > i + 1 && end - i < 50 && end < text.size()) {
                std::string name = text.substr(i + 1, end - (i + 1));
                bool valid = !name.empty() && name.find(' ') == std::string::npos &&
                             name.find('\n') == std::string::npos;
                for (char ec : name) {
                    if (!std::isalnum(ec) && ec != '_' && ec != '-' && ec != '+') {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    flushCurrent();
                    TextSpan span{TextSpan::Style::Emoji, ":" + name + ":"};
                    span.reference = name;
                    spans.push_back(span);
                    i = end + 1;
                    continue;
                }
            }
        }

        // bold: *text* (same line, non-empty content)
        if (c == '*') {
            size_t end = findClosingMarker(text, i + 1, '*');
            if (end != std::string::npos && end > i + 1) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Bold, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
                continue;
            }
        }

        // italic: _text_ (same line)
        if (c == '_') {
            size_t end = findClosingMarker(text, i + 1, '_');
            if (end != std::string::npos && end > i + 1) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Italic, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
                continue;
            }
        }

        // strikethrough: ~text~ (same line)
        if (c == '~') {
            size_t end = findClosingMarker(text, i + 1, '~');
            if (end != std::string::npos && end > i + 1) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Strike, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
                continue;
            }
        }

        // block quote: > at start of line
        if (c == '>' && (i == 0 || text[i - 1] == '\n') && i + 1 < text.size() && text[i + 1] == ' ') {
            flushCurrent();
            size_t eol = text.find('\n', i + 2);
            std::string quote_text = (eol != std::string::npos) ? text.substr(i + 2, eol - (i + 2)) : text.substr(i + 2);
            spans.push_back({TextSpan::Style::Quote, quote_text});
            i = (eol != std::string::npos) ? eol + 1 : text.size();
            continue;
        }

        current += c;
        i++;
    }

    flushCurrent();
    return spans;
}

// render spans with proper inline layout.
// tracks cursor position and wraps naturally. newline spans force line breaks.
void renderSpans(const std::vector<TextSpan>& spans, float wrap_width,
                 const ImVec4& default_color) {
    if (spans.empty()) return;

    float start_x = ImGui::GetCursorPosX();
    bool need_sameline = false;

    for (auto& span : spans) {
        // newlines break the inline flow
        if (span.style == TextSpan::Style::Normal && span.text == "\n") {
            need_sameline = false;
            ImGui::NewLine();
            ImGui::SetCursorPosX(start_x);
            continue;
        }

        // code blocks and quotes always start on their own line
        if (span.style == TextSpan::Style::CodeBlock || span.style == TextSpan::Style::Quote) {
            need_sameline = false;
        }

        if (need_sameline) {
            ImGui::SameLine(0, 0);
        }

        ImVec4 color = default_color;
        if (span.color.x != 0.8f || span.color.y != 0.8f || span.color.z != 0.8f) {
            color = span.color;
        }

        switch (span.style) {
        case TextSpan::Style::Normal:
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushTextWrapPos(start_x + wrap_width);
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            need_sameline = true;
            break;

        case TextSpan::Style::Bold:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            need_sameline = true;
            break;

        case TextSpan::Style::Italic:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.7f, 0.75f, 0.8f, 1.0f});
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            need_sameline = true;
            break;

        case TextSpan::Style::Strike:
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.5f, 0.5f, 0.55f, 1.0f});
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
                ImVec2 text_size = ImGui::CalcTextSize(span.text.c_str());
                float y_mid = pos.y + text_size.y * 0.5f;
                ImGui::GetWindowDrawList()->AddLine(
                    {pos.x, y_mid}, {pos.x + text_size.x, y_mid},
                    ImGui::ColorConvertFloat4ToU32({0.5f, 0.5f, 0.55f, 1.0f}));
                need_sameline = true;
            }
            break;

        case TextSpan::Style::Code:
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 text_size = ImGui::CalcTextSize(span.text.c_str());
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {pos.x - 2, pos.y - 1}, {pos.x + text_size.x + 2, pos.y + text_size.y + 1},
                    ImGui::ColorConvertFloat4ToU32({0.08f, 0.08f, 0.11f, 1.0f}));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.9f, 0.6f, 0.5f, 1.0f});
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
                need_sameline = true;
            }
            break;

        case TextSpan::Style::CodeBlock:
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 text_size = ImGui::CalcTextSize(span.text.c_str(), nullptr, false, wrap_width - 16);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {pos.x - 4, pos.y - 2},
                    {pos.x + wrap_width, pos.y + text_size.y + 8},
                    ImGui::ColorConvertFloat4ToU32({0.08f, 0.08f, 0.11f, 1.0f}));
                ImGui::SetCursorScreenPos({pos.x + 4, pos.y + 2});
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.85f, 0.85f, 0.8f, 1.0f});
                ImGui::PushTextWrapPos(start_x + wrap_width - 8);
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                ImGui::SetCursorScreenPos({pos.x, pos.y + text_size.y + 10});
                need_sameline = false;
            }
            break;

        case TextSpan::Style::Link:
            {
                ImGui::PushStyleColor(ImGuiCol_Text, span.color);
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
                ImVec2 mn = ImGui::GetItemRectMin();
                ImVec2 mx = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddLine(
                    {mn.x, mx.y}, {mx.x, mx.y},
                    ImGui::ColorConvertFloat4ToU32(span.color));
                need_sameline = true;
            }
            break;

        case TextSpan::Style::Mention:
        case TextSpan::Style::ChannelRef:
        case TextSpan::Style::BroadcastMention:
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 text_size = ImGui::CalcTextSize(span.text.c_str());
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {pos.x - 1, pos.y}, {pos.x + text_size.x + 1, pos.y + text_size.y},
                    ImGui::ColorConvertFloat4ToU32({0.15f, 0.25f, 0.4f, 0.5f}), 2.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, span.color);
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
                need_sameline = true;
            }
            break;

        case TextSpan::Style::Emoji:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 0.9f, 0.4f, 1.0f});
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            need_sameline = true;
            break;

        case TextSpan::Style::Quote:
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {pos.x, pos.y}, {pos.x + 3, pos.y + ImGui::GetTextLineHeight()},
                    ImGui::ColorConvertFloat4ToU32({0.4f, 0.6f, 0.4f, 0.8f}));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.6f, 0.65f, 0.6f, 1.0f});
                ImGui::PushTextWrapPos(start_x + wrap_width);
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                need_sameline = false;
            }
            break;
        }
    }
}

void renderColoredText(const std::string& text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
}

} // namespace conduit::render
