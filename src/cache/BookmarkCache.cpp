#include "cache/BookmarkCache.h"
#include "util/Logger.h"
#include <nlohmann/json.hpp>

namespace conduit::cache {

BookmarkCache::BookmarkCache(Database& db) : db_(db) {}

void BookmarkCache::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    bookmarks_.clear();

    db_.query("SELECT id, channel_id, title, link, emoji, type, created_by, date_created "
              "FROM bookmarks",
              [&](int, char** vals, char**) {
                  slack::Bookmark b;
                  b.id = vals[0] ? vals[0] : "";
                  b.channel_id = vals[1] ? vals[1] : "";
                  b.title = vals[2] ? vals[2] : "";
                  b.link = vals[3] ? vals[3] : "";
                  b.emoji = vals[4] ? vals[4] : "";
                  b.type = vals[5] ? vals[5] : "link";
                  b.created_by = vals[6] ? vals[6] : "";
                  b.date_created = vals[7] ? std::atoll(vals[7]) : 0;
                  bookmarks_[b.channel_id].push_back(b);
              });

    LOG_DEBUG("loaded bookmarks from DB");
}

void BookmarkCache::loadForChannel(const slack::ChannelId& channel,
                                    const std::vector<slack::Bookmark>& bookmarks) {
    std::lock_guard<std::mutex> lock(mutex_);
    bookmarks_[channel] = bookmarks;

    // persist
    db_.exec("DELETE FROM bookmarks WHERE channel_id = '" + channel + "'");
    for (auto& b : bookmarks) {
        std::string sql = "INSERT OR REPLACE INTO bookmarks "
                          "(id, channel_id, title, link, emoji, type, created_by, date_created) VALUES ("
                          "'" + b.id + "', '" + b.channel_id + "', '" + b.title + "', "
                          "'" + b.link + "', '" + b.emoji + "', '" + b.type + "', "
                          "'" + b.created_by + "', " + std::to_string(b.date_created) + ")";
        db_.exec(sql);
    }
}

void BookmarkCache::add(const slack::Bookmark& bookmark) {
    std::lock_guard<std::mutex> lock(mutex_);
    bookmarks_[bookmark.channel_id].push_back(bookmark);
}

void BookmarkCache::remove(const std::string& bookmark_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, bmarks] : bookmarks_) {
        bmarks.erase(std::remove_if(bmarks.begin(), bmarks.end(),
                     [&](const slack::Bookmark& b) { return b.id == bookmark_id; }),
                     bmarks.end());
    }
    db_.exec("DELETE FROM bookmarks WHERE id = '" + bookmark_id + "'");
}

std::vector<slack::Bookmark> BookmarkCache::getForChannel(const slack::ChannelId& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = bookmarks_.find(channel);
    if (it != bookmarks_.end()) return it->second;
    return {};
}

void BookmarkCache::flush() {
    // already flushed in loadForChannel
}

} // namespace conduit::cache
