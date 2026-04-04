#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace conduit::cache {

class ChannelCache {
public:
    explicit ChannelCache(Database& db);

    // bulk load from API response
    void loadFromAPI(const std::vector<slack::Channel>& channels);

    // single channel operations
    void upsert(const slack::Channel& channel);
    std::optional<slack::Channel> get(const slack::ChannelId& id) const;
    std::vector<slack::Channel> getAll(bool include_archived = false) const;
    std::vector<slack::Channel> getJoined() const;

    // update specific fields
    void updateUnreadCount(const slack::ChannelId& id, int count);
    void updateLastRead(const slack::ChannelId& id, const slack::Timestamp& ts);
    void updateTopic(const slack::ChannelId& id, const std::string& topic);

    // persist to sqlite
    void flush();

    // load from sqlite
    void loadFromDB();

private:
    Database& db_;
    mutable std::mutex mutex_;
    std::unordered_map<slack::ChannelId, slack::Channel> channels_;

    static std::string channelTypeStr(slack::ChannelType type);
    static slack::ChannelType parseChannelType(const std::string& s);
};

} // namespace conduit::cache
