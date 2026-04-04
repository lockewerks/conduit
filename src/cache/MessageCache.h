#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace conduit::cache {

class MessageCache {
public:
    explicit MessageCache(Database& db);

    // store messages (from history fetch or real-time)
    void store(const slack::ChannelId& channel, const std::vector<slack::Message>& messages);
    void store(const slack::ChannelId& channel, const slack::Message& message);

    // get messages for a channel (newest last)
    std::vector<slack::Message> get(const slack::ChannelId& channel, int limit = 100,
                                     const slack::Timestamp& before = "") const;

    // get thread replies
    std::vector<slack::Message> getThread(const slack::ChannelId& channel,
                                           const slack::Timestamp& thread_ts) const;

    // update a specific message
    void update(const slack::ChannelId& channel, const slack::Message& message);

    // delete a specific message
    void remove(const slack::ChannelId& channel, const slack::Timestamp& ts);

    // add/remove reaction on a message
    void addReaction(const slack::ChannelId& channel, const slack::Timestamp& ts,
                     const slack::Reaction& reaction);
    void removeReaction(const slack::ChannelId& channel, const slack::Timestamp& ts,
                        const std::string& emoji, const slack::UserId& user);

    // pin/unpin
    void setPin(const slack::ChannelId& channel, const slack::Timestamp& ts, bool pinned);

    // persist to sqlite
    void flush(const slack::ChannelId& channel);
    void flushAll();

    // load from sqlite
    void loadFromDB(const slack::ChannelId& channel, int limit = 200);

private:
    Database& db_;
    mutable std::mutex mutex_;

    // in-memory message store per channel
    std::unordered_map<slack::ChannelId, std::vector<slack::Message>> messages_;

    void insertSorted(std::vector<slack::Message>& vec, const slack::Message& msg);
    std::string reactionsToJson(const std::vector<slack::Reaction>& reactions) const;
    std::vector<slack::Reaction> jsonToReactions(const std::string& json) const;
};

} // namespace conduit::cache
