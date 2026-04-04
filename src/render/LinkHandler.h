#pragma once
#include <string>
#include <functional>

namespace conduit::render {

// handles clickable things in the message view: URLs, @mentions, #channels
class LinkHandler {
public:
    using URLCallback = std::function<void(const std::string& url)>;
    using MentionCallback = std::function<void(const std::string& user_id)>;
    using ChannelCallback = std::function<void(const std::string& channel_id)>;

    void setURLHandler(URLCallback cb) { url_cb_ = std::move(cb); }
    void setMentionHandler(MentionCallback cb) { mention_cb_ = std::move(cb); }
    void setChannelHandler(ChannelCallback cb) { channel_cb_ = std::move(cb); }

    void openURL(const std::string& url);
    void openMention(const std::string& user_id);
    void openChannel(const std::string& channel_id);

private:
    URLCallback url_cb_;
    MentionCallback mention_cb_;
    ChannelCallback channel_cb_;
};

} // namespace conduit::render
