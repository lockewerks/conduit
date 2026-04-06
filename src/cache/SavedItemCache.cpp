#include "cache/SavedItemCache.h"
#include "util/Logger.h"

namespace conduit::cache {

SavedItemCache::SavedItemCache(Database& db) : db_(db) {}

void SavedItemCache::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();

    db_.query("SELECT channel_id, message_ts, date_saved FROM saved_items ORDER BY date_saved DESC",
              [&](int, char** vals, char**) {
                  slack::SavedItem si;
                  si.type = "message";
                  si.channel_id = vals[0] ? vals[0] : "";
                  si.message_ts = vals[1] ? vals[1] : "";
                  si.date_saved = vals[2] ? std::atoll(vals[2]) : 0;
                  items_.push_back(si);
              });

    LOG_DEBUG("loaded " + std::to_string(items_.size()) + " saved items from DB");
}

void SavedItemCache::loadFromAPI(const std::vector<slack::SavedItem>& items) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_ = items;
    flush();
}

void SavedItemCache::add(const slack::SavedItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.insert(items_.begin(), item);
    db_.exec("INSERT OR REPLACE INTO saved_items (channel_id, message_ts) VALUES ("
             "'" + item.channel_id + "', '" + item.message_ts + "')");
}

void SavedItemCache::remove(const slack::ChannelId& channel, const slack::Timestamp& ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.erase(
        std::remove_if(items_.begin(), items_.end(),
                       [&](const slack::SavedItem& si) {
                           return si.channel_id == channel && si.message_ts == ts;
                       }),
        items_.end());
    db_.exec("DELETE FROM saved_items WHERE channel_id = '" + channel +
             "' AND message_ts = '" + ts + "'");
}

bool SavedItemCache::isSaved(const slack::ChannelId& channel, const slack::Timestamp& ts) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& si : items_) {
        if (si.channel_id == channel && si.message_ts == ts) return true;
    }
    return false;
}

std::vector<slack::SavedItem> SavedItemCache::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_;
}

void SavedItemCache::flush() {
    db_.exec("DELETE FROM saved_items");
    for (auto& si : items_) {
        db_.exec("INSERT INTO saved_items (channel_id, message_ts, date_saved) VALUES ("
                 "'" + si.channel_id + "', '" + si.message_ts + "', "
                 + std::to_string(si.date_saved) + ")");
    }
}

} // namespace conduit::cache
