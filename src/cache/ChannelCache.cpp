#include "cache/ChannelCache.h"
#include "util/Logger.h"

namespace conduit::cache {

ChannelCache::ChannelCache(Database& db) : db_(db) {}

void ChannelCache::loadFromAPI(const std::vector<slack::Channel>& channels) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& ch : channels) {
        channels_[ch.id] = ch;
    }
    LOG_INFO("loaded " + std::to_string(channels.size()) + " channels from API");
    flush();
}

void ChannelCache::upsert(const slack::Channel& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_[channel.id] = channel;
}

std::optional<slack::Channel> ChannelCache::get(const slack::ChannelId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(id);
    if (it != channels_.end()) return it->second;
    return std::nullopt;
}

std::vector<slack::Channel> ChannelCache::getAll(bool include_archived) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<slack::Channel> result;
    for (auto& [_, ch] : channels_) {
        if (!include_archived && ch.is_archived) continue;
        result.push_back(ch);
    }
    return result;
}

std::vector<slack::Channel> ChannelCache::getJoined() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<slack::Channel> result;
    for (auto& [_, ch] : channels_) {
        if (ch.is_member && !ch.is_archived) {
            result.push_back(ch);
        }
    }
    // sort: channels first (alphabetical), then DMs
    std::sort(result.begin(), result.end(), [](const slack::Channel& a, const slack::Channel& b) {
        bool a_is_dm = (a.type == slack::ChannelType::DirectMessage ||
                        a.type == slack::ChannelType::MultiPartyDM);
        bool b_is_dm = (b.type == slack::ChannelType::DirectMessage ||
                        b.type == slack::ChannelType::MultiPartyDM);
        if (a_is_dm != b_is_dm) return !a_is_dm; // channels before DMs
        return a.name < b.name;
    });
    return result;
}

void ChannelCache::updateUnreadCount(const slack::ChannelId& id, int count) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(id);
    if (it != channels_.end()) {
        it->second.unread_count = count;
        it->second.has_unreads = (count > 0);
    }
}

void ChannelCache::updateLastRead(const slack::ChannelId& id, const slack::Timestamp& ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(id);
    if (it != channels_.end()) {
        it->second.last_read = ts;
    }
}

void ChannelCache::updateTopic(const slack::ChannelId& id, const std::string& topic) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(id);
    if (it != channels_.end()) {
        it->second.topic = topic;
    }
}

void ChannelCache::flush() {
    // write all channels to sqlite
    for (auto& [_, ch] : channels_) {
        std::string sql = "INSERT OR REPLACE INTO channels "
                          "(id, name, topic, purpose, type, is_member, is_muted, "
                          "is_archived, member_count, dm_user_id, last_read) VALUES ("
                          "'" + ch.id + "', "
                          "'" + ch.name + "', "
                          "'" + ch.topic + "', "
                          "'" + ch.purpose + "', "
                          "'" + channelTypeStr(ch.type) + "', "
                          + std::to_string(ch.is_member ? 1 : 0) + ", "
                          + std::to_string(ch.is_muted ? 1 : 0) + ", "
                          + std::to_string(ch.is_archived ? 1 : 0) + ", "
                          + std::to_string(ch.member_count) + ", "
                          "'" + ch.dm_user_id + "', "
                          "'" + ch.last_read + "')";
        db_.exec(sql);
    }
}

void ChannelCache::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.clear();

    db_.query("SELECT id, name, topic, purpose, type, is_member, is_muted, "
              "is_archived, member_count, dm_user_id, last_read FROM channels",
              [&](int, char** vals, char**) {
                  slack::Channel ch;
                  ch.id = vals[0] ? vals[0] : "";
                  ch.name = vals[1] ? vals[1] : "";
                  ch.topic = vals[2] ? vals[2] : "";
                  ch.purpose = vals[3] ? vals[3] : "";
                  ch.type = parseChannelType(vals[4] ? vals[4] : "public");
                  ch.is_member = vals[5] && std::atoi(vals[5]);
                  ch.is_muted = vals[6] && std::atoi(vals[6]);
                  ch.is_archived = vals[7] && std::atoi(vals[7]);
                  ch.member_count = vals[8] ? std::atoi(vals[8]) : 0;
                  ch.dm_user_id = vals[9] ? vals[9] : "";
                  ch.last_read = vals[10] ? vals[10] : "";
                  channels_[ch.id] = ch;
              });

    LOG_DEBUG("loaded " + std::to_string(channels_.size()) + " channels from DB");
}

std::string ChannelCache::channelTypeStr(slack::ChannelType type) {
    switch (type) {
    case slack::ChannelType::PublicChannel: return "public";
    case slack::ChannelType::PrivateChannel: return "private";
    case slack::ChannelType::DirectMessage: return "dm";
    case slack::ChannelType::MultiPartyDM: return "mpdm";
    case slack::ChannelType::GroupDM: return "group";
    }
    return "public";
}

slack::ChannelType ChannelCache::parseChannelType(const std::string& s) {
    if (s == "private") return slack::ChannelType::PrivateChannel;
    if (s == "dm") return slack::ChannelType::DirectMessage;
    if (s == "mpdm") return slack::ChannelType::MultiPartyDM;
    if (s == "group") return slack::ChannelType::GroupDM;
    return slack::ChannelType::PublicChannel;
}

} // namespace conduit::cache
