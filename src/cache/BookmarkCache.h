#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace conduit::cache {

class BookmarkCache {
public:
    explicit BookmarkCache(Database& db);

    void loadFromDB();
    void loadForChannel(const slack::ChannelId& channel, const std::vector<slack::Bookmark>& bookmarks);
    void add(const slack::Bookmark& bookmark);
    void remove(const std::string& bookmark_id);
    std::vector<slack::Bookmark> getForChannel(const slack::ChannelId& channel) const;
    void flush();

private:
    Database& db_;
    mutable std::mutex mutex_;
    // channel_id -> bookmarks
    std::unordered_map<slack::ChannelId, std::vector<slack::Bookmark>> bookmarks_;
};

} // namespace conduit::cache
