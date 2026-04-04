#pragma once
#include <string>
#include <vector>
#include <imgui.h>

namespace conduit::render {

// a styled text span - the output of the mrkdwn parser
struct TextSpan {
    enum class Style {
        Normal, Bold, Italic, Strike, Code, CodeBlock,
        Link, Mention, ChannelRef, Emoji, Quote, BroadcastMention
    };

    Style style = Style::Normal;
    std::string text;
    std::string link_url;    // for links
    std::string reference;   // user_id, channel_id, or emoji name
    ImVec4 color = {0.8f, 0.8f, 0.8f, 1.0f};
};

// parse slack's mrkdwn format into styled spans
// this handles bold, italic, code, links, mentions, emoji, the whole thing
std::vector<TextSpan> parseMrkdwn(const std::string& text);

// render a list of spans using imgui
// handles inline layout, word wrapping, different styles
void renderSpans(const std::vector<TextSpan>& spans, float wrap_width,
                 const ImVec4& default_color);

// simple utility: render a single line of colored text
void renderColoredText(const std::string& text, const ImVec4& color);

} // namespace conduit::render
