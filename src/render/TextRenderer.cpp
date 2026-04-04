#include "render/TextRenderer.h"
#include <regex>

namespace conduit::render {

// the mrkdwn parser. slack's markdown variant is... special.
// it looks like markdown but isn't really. *bold* uses single asterisks,
// _italic_ uses underscores, ~strike~ uses tildes. links are <url|text>.
// mentions are <@U1234>. channels are <#C1234|name>. fun times.

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

        // code blocks (```)
        if (c == '`' && i + 2 < text.size() && text[i + 1] == '`' && text[i + 2] == '`') {
            flushCurrent();
            size_t end = text.find("```", i + 3);
            if (end != std::string::npos) {
                std::string code = text.substr(i + 3, end - (i + 3));
                // strip leading newline if present
                if (!code.empty() && code[0] == '\n') code = code.substr(1);
                spans.push_back({TextSpan::Style::CodeBlock, code});
                i = end + 3;
            } else {
                current += "```";
                i += 3;
            }
            continue;
        }

        // inline code (`)
        if (c == '`') {
            flushCurrent();
            size_t end = text.find('`', i + 1);
            if (end != std::string::npos) {
                spans.push_back({TextSpan::Style::Code, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
            } else {
                current += '`';
                i++;
            }
            continue;
        }

        // slack-style links and references: <...>
        if (c == '<') {
            size_t end = text.find('>', i + 1);
            if (end != std::string::npos) {
                flushCurrent();
                std::string inner = text.substr(i + 1, end - (i + 1));

                if (inner.starts_with("@U") || inner.starts_with("@W")) {
                    // user mention: <@U1234> or <@U1234|display_name>
                    auto pipe = inner.find('|');
                    std::string uid = (pipe != std::string::npos)
                                          ? inner.substr(1, pipe - 1)
                                          : inner.substr(1);
                    std::string display = (pipe != std::string::npos)
                                              ? "@" + inner.substr(pipe + 1)
                                              : "@" + uid;
                    TextSpan span{TextSpan::Style::Mention, display};
                    span.reference = uid;
                    span.color = {0.47f, 0.78f, 1.0f, 1.0f};
                    spans.push_back(span);
                } else if (inner.starts_with("#C")) {
                    // channel reference: <#C1234|channel-name>
                    auto pipe = inner.find('|');
                    std::string cid = (pipe != std::string::npos)
                                          ? inner.substr(1, pipe - 1)
                                          : inner.substr(1);
                    std::string name = (pipe != std::string::npos)
                                           ? "#" + inner.substr(pipe + 1)
                                           : "#" + cid;
                    TextSpan span{TextSpan::Style::ChannelRef, name};
                    span.reference = cid;
                    span.color = {0.47f, 0.78f, 1.0f, 1.0f};
                    spans.push_back(span);
                } else if (inner.starts_with("!")) {
                    // broadcast mentions: <!here>, <!channel>, <!everyone>
                    std::string broadcast = inner.substr(1);
                    auto pipe = broadcast.find('|');
                    if (pipe != std::string::npos) broadcast = broadcast.substr(0, pipe);
                    TextSpan span{TextSpan::Style::BroadcastMention, "@" + broadcast};
                    span.color = {1.0f, 0.85f, 0.4f, 1.0f}; // yellow highlight
                    spans.push_back(span);
                } else {
                    // URL: <https://...|label> or just <https://...>
                    auto pipe = inner.find('|');
                    std::string url = (pipe != std::string::npos)
                                          ? inner.substr(0, pipe)
                                          : inner;
                    std::string label = (pipe != std::string::npos)
                                            ? inner.substr(pipe + 1)
                                            : url;
                    TextSpan span{TextSpan::Style::Link, label};
                    span.link_url = url;
                    span.color = {0.4f, 0.6f, 1.0f, 1.0f};
                    spans.push_back(span);
                }

                i = end + 1;
                continue;
            }
        }

        // emoji :name:
        if (c == ':' && i + 1 < text.size() && text[i + 1] != ' ') {
            size_t end = text.find(':', i + 1);
            if (end != std::string::npos && end - i < 50) {
                std::string name = text.substr(i + 1, end - (i + 1));
                // basic validation: emoji names are alphanumeric with underscores/hyphens
                bool valid = !name.empty();
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

        // bold: *text*
        if (c == '*') {
            size_t end = text.find('*', i + 1);
            if (end != std::string::npos && end > i + 1) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Bold, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
                continue;
            }
        }

        // italic: _text_
        if (c == '_') {
            size_t end = text.find('_', i + 1);
            if (end != std::string::npos && end > i + 1) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Italic, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
                continue;
            }
        }

        // strikethrough: ~text~
        if (c == '~') {
            size_t end = text.find('~', i + 1);
            if (end != std::string::npos && end > i + 1) {
                flushCurrent();
                spans.push_back({TextSpan::Style::Strike, text.substr(i + 1, end - (i + 1))});
                i = end + 1;
                continue;
            }
        }

        // block quote: > at start of line
        if (c == '>' && (i == 0 || text[i - 1] == '\n') && i + 1 < text.size() &&
            text[i + 1] == ' ') {
            flushCurrent();
            // grab until end of line
            size_t eol = text.find('\n', i + 2);
            std::string quote_text =
                (eol != std::string::npos) ? text.substr(i + 2, eol - (i + 2)) : text.substr(i + 2);
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

void renderSpans(const std::vector<TextSpan>& spans, float wrap_width,
                 const ImVec4& default_color) {
    float cursor_x = ImGui::GetCursorPosX();
    float start_x = cursor_x;
    bool first = true;

    for (auto& span : spans) {
        if (!first && span.style != TextSpan::Style::CodeBlock &&
            span.style != TextSpan::Style::Quote) {
            ImGui::SameLine(0, 0);
        }
        first = false;

        ImVec4 color = (span.color.x != 0.8f || span.color.y != 0.8f)
                            ? span.color
                            : default_color;

        switch (span.style) {
        case TextSpan::Style::Normal:
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushTextWrapPos(start_x + wrap_width);
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            break;

        case TextSpan::Style::Bold:
            // imgui doesn't have bold in the default font, so we use bright white
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            break;

        case TextSpan::Style::Italic:
            // no real italic either, use a slightly different color
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.7f, 0.75f, 0.8f, 1.0f});
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            break;

        case TextSpan::Style::Strike:
            // strikethrough is tricky in imgui, we'll draw a line through it
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.5f, 0.5f, 0.55f, 1.0f});
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
                // draw the strikethrough line
                ImVec2 text_size = ImGui::CalcTextSize(span.text.c_str());
                float y_mid = pos.y + text_size.y * 0.5f;
                ImGui::GetWindowDrawList()->AddLine(
                    {pos.x, y_mid}, {pos.x + text_size.x, y_mid},
                    ImGui::ColorConvertFloat4ToU32({0.5f, 0.5f, 0.55f, 1.0f}));
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
            }
            break;

        case TextSpan::Style::CodeBlock:
            {
                first = true; // next span should not SameLine
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.85f, 0.85f, 0.8f, 1.0f});
                ImGui::PushTextWrapPos(start_x + wrap_width);
                // background for the code block
                ImVec2 text_size = ImGui::CalcTextSize(span.text.c_str(), nullptr, false,
                                                        wrap_width);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {pos.x - 4, pos.y - 2},
                    {pos.x + wrap_width, pos.y + text_size.y + 4},
                    ImGui::ColorConvertFloat4ToU32({0.08f, 0.08f, 0.11f, 1.0f}));
                ImGui::SetCursorScreenPos({pos.x + 4, pos.y + 2});
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
            break;

        case TextSpan::Style::Link:
            ImGui::PushStyleColor(ImGuiCol_Text, span.color);
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            // underline
            {
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddLine(
                    {min.x, max.y}, {max.x, max.y},
                    ImGui::ColorConvertFloat4ToU32(span.color));
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
                    ImGui::ColorConvertFloat4ToU32({0.15f, 0.25f, 0.4f, 0.5f}),
                    2.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, span.color);
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
            }
            break;

        case TextSpan::Style::Emoji:
            // for now just render the :name: text, real emoji rendering comes in phase 3
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 0.9f, 0.4f, 1.0f});
            ImGui::TextUnformatted(span.text.c_str());
            ImGui::PopStyleColor();
            break;

        case TextSpan::Style::Quote:
            {
                first = true;
                ImVec2 pos = ImGui::GetCursorScreenPos();
                // draw the left border for the quote
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {pos.x, pos.y}, {pos.x + 3, pos.y + ImGui::GetTextLineHeight()},
                    ImGui::ColorConvertFloat4ToU32({0.4f, 0.6f, 0.4f, 0.8f}));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.6f, 0.65f, 0.6f, 1.0f});
                ImGui::TextUnformatted(span.text.c_str());
                ImGui::PopStyleColor();
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
