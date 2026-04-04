#include "render/LinkHandler.h"
#include "util/Platform.h"
#include "util/Logger.h"

namespace conduit::render {

void LinkHandler::openURL(const std::string& url) {
    if (url_cb_) {
        url_cb_(url);
    } else {
        // default: open in browser. we're a text client, not a browser engine.
        conduit::platform::openURL(url);
    }
}

void LinkHandler::openMention(const std::string& user_id) {
    if (mention_cb_) mention_cb_(user_id);
}

void LinkHandler::openChannel(const std::string& channel_id) {
    if (channel_cb_) channel_cb_(channel_id);
}

} // namespace conduit::render
