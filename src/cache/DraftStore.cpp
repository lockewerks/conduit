#include "cache/DraftStore.h"
#include "util/Logger.h"

namespace conduit::cache {

DraftStore::DraftStore(Database& db) : db_(db) {}

void DraftStore::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    drafts_.clear();

    db_.query("SELECT channel_id, text, thread_ts FROM drafts",
              [&](int, char** vals, char**) {
                  Draft d;
                  std::string channel = vals[0] ? vals[0] : "";
                  d.text = vals[1] ? vals[1] : "";
                  d.thread_ts = vals[2] ? vals[2] : "";
                  if (!channel.empty() && !d.text.empty()) {
                      drafts_[channel] = d;
                  }
              });

    LOG_DEBUG("loaded " + std::to_string(drafts_.size()) + " drafts from DB");
}

void DraftStore::save(const slack::ChannelId& channel, const std::string& text,
                       const slack::Timestamp& thread_ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (text.empty()) {
        drafts_.erase(channel);
        db_.exec("DELETE FROM drafts WHERE channel_id = '" + channel + "'");
        return;
    }
    drafts_[channel] = {text, thread_ts};
    db_.exec("INSERT OR REPLACE INTO drafts (channel_id, text, thread_ts, updated_at) VALUES ("
             "'" + channel + "', '" + text + "', '" + thread_ts + "', "
             "strftime('%s', 'now'))");
}

Draft DraftStore::get(const slack::ChannelId& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = drafts_.find(channel);
    if (it != drafts_.end()) return it->second;
    return {};
}

void DraftStore::remove(const slack::ChannelId& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    drafts_.erase(channel);
    db_.exec("DELETE FROM drafts WHERE channel_id = '" + channel + "'");
}

bool DraftStore::has(const slack::ChannelId& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return drafts_.count(channel) > 0;
}

} // namespace conduit::cache
