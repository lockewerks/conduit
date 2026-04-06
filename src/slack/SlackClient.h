#pragma once
#include "slack/Types.h"
#include "slack/WebAPI.h"
#include "slack/SlackAuth.h"
#include "slack/SocketModeClient.h"
#include "slack/SlackWebSocket.h"
#include "slack/EventDispatcher.h"
#include "slack/RateLimiter.h"
#include "cache/Database.h"
#include "cache/ChannelCache.h"
#include "cache/UserCache.h"
#include "app/Config.h"

#include <memory>
#include <string>

namespace conduit::slack {

// the main slack client for a single org
// owns the API connection, socket mode, caches, the works
class SlackClient {
public:
    SlackClient(const OrgConfig& config, cache::Database& db);
    ~SlackClient();

    // lifecycle
    bool connect();
    void disconnect();
    bool isConnected() const;
    TeamId teamId() const { return auth_info_.team_id; }
    std::string teamName() const { return auth_info_.team_name; }
    UserId selfUserId() const { return auth_info_.user_id; }

    // channel operations
    std::vector<Channel> getChannels(bool include_archived = false);
    std::optional<Channel> getChannel(const ChannelId& id);
    bool joinChannel(const ChannelId& id);
    bool leaveChannel(const ChannelId& id);
    bool markRead(const ChannelId& channel, const Timestamp& ts);
    bool setTopic(const ChannelId& channel, const std::string& topic);
    // open (or find) a DM channel with a user. returns channel ID on success.
    std::optional<ChannelId> openDM(const UserId& user_id);

    // message operations
    std::vector<Message> getHistory(const ChannelId& channel, int limit = 100,
                                     const std::optional<Timestamp>& before = std::nullopt);
    std::vector<Message> getThreadReplies(const ChannelId& channel, const Timestamp& thread_ts);
    bool sendMessage(const ChannelId& channel, const std::string& text,
                     const std::optional<Timestamp>& thread_ts = std::nullopt,
                     bool reply_broadcast = false);
    bool editMessage(const ChannelId& channel, const Timestamp& ts, const std::string& new_text);
    bool deleteMessage(const ChannelId& channel, const Timestamp& ts);
    bool addReaction(const ChannelId& channel, const Timestamp& ts, const std::string& emoji);
    bool removeReaction(const ChannelId& channel, const Timestamp& ts, const std::string& emoji);

    // file operations
    bool uploadFile(const ChannelId& channel, const std::string& filepath,
                    const std::string& title = "");
    std::vector<uint8_t> downloadFile(const std::string& url);

    // user operations
    std::optional<User> getUser(const UserId& id);
    std::string displayName(const UserId& id);

    // search
    struct SearchResult {
        Message message;
        ChannelId channel;
        std::string channel_name;
    };
    std::vector<SearchResult> searchMessages(const std::string& query, int count = 20);

    // pins
    std::vector<Message> getPins(const ChannelId& channel);
    bool pinMessage(const ChannelId& channel, const Timestamp& ts);
    bool unpinMessage(const ChannelId& channel, const Timestamp& ts);

    // emoji
    std::unordered_map<std::string, std::string> getCustomEmoji();

    // typing
    void sendTyping(const ChannelId& channel);

    // status
    bool setStatus(const std::string& emoji, const std::string& text, int expiration_minutes = 0);
    bool setPresence(bool is_away);

    // event queue (consumed by main thread)
    bool pollEvent(SlackEvent& event_out);

    // caches (for UI to read from)
    cache::ChannelCache& channelCache() { return channel_cache_; }
    cache::UserCache& userCache() { return user_cache_; }

    std::string connectionState() const;

private:
    OrgConfig config_;
    AuthInfo auth_info_;

    std::unique_ptr<WebAPI> api_;
    std::unique_ptr<SocketModeClient> socket_;       // socket mode (app token)
    std::unique_ptr<SlackWebSocket> web_socket_;     // direct websocket (xoxc token)
    std::unique_ptr<EventDispatcher> dispatcher_;
    RateLimiter rate_limiter_;

    cache::Database& db_;
    cache::ChannelCache channel_cache_;
    cache::UserCache user_cache_;

    ThreadSafeQueue<SlackEvent> event_queue_;

    // fetch paginated results from slack API
    template <typename T>
    std::vector<T> fetchPaginated(const std::string& method, const std::string& list_key,
                                   const std::string& extra_params = "");

    void bootstrapData();
};

} // namespace conduit::slack
