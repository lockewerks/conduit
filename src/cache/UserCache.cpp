#include "cache/UserCache.h"
#include "util/Logger.h"

namespace conduit::cache {

UserCache::UserCache(Database& db) : db_(db) {}

void UserCache::loadFromAPI(const std::vector<slack::User>& users) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& u : users) {
        users_[u.id] = u;
    }
    LOG_INFO("loaded " + std::to_string(users.size()) + " users from API");
    flush();
}

void UserCache::upsert(const slack::User& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    users_[user.id] = user;
}

std::optional<slack::User> UserCache::get(const slack::UserId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    if (it != users_.end()) return it->second;
    return std::nullopt;
}

std::vector<slack::User> UserCache::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<slack::User> result;
    result.reserve(users_.size());
    for (auto& [_, u] : users_) {
        result.push_back(u);
    }
    return result;
}

std::string UserCache::displayName(const slack::UserId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    if (it != users_.end()) {
        return it->second.effectiveName();
    }
    return id; // unknown user, just show the ID
}

void UserCache::setOnline(const slack::UserId& id, bool online) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    if (it != users_.end()) {
        it->second.is_online = online;
    }
}

bool UserCache::isOnline(const slack::UserId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    return it != users_.end() && it->second.is_online;
}

void UserCache::flush() {
    for (auto& [_, u] : users_) {
        // yeah this is string concatenation for SQL. fight me.
        // we'll switch to prepared statements when this becomes a bottleneck
        // (it won't, sqlite is fast enough for this)
        std::string sql = "INSERT OR REPLACE INTO users "
                          "(id, display_name, real_name, avatar_url_72, avatar_url_192, "
                          "status_emoji, status_text, is_bot) VALUES ("
                          "'" + u.id + "', "
                          "'" + u.display_name + "', "
                          "'" + u.real_name + "', "
                          "'" + u.avatar_url_72 + "', "
                          "'" + u.avatar_url_192 + "', "
                          "'" + u.status_emoji + "', "
                          "'" + u.status_text + "', "
                          + std::to_string(u.is_bot ? 1 : 0) + ")";
        db_.exec(sql);
    }
}

void UserCache::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.clear();

    db_.query("SELECT id, display_name, real_name, avatar_url_72, avatar_url_192, "
              "status_emoji, status_text, is_bot FROM users",
              [&](int, char** vals, char**) {
                  slack::User u;
                  u.id = vals[0] ? vals[0] : "";
                  u.display_name = vals[1] ? vals[1] : "";
                  u.real_name = vals[2] ? vals[2] : "";
                  u.avatar_url_72 = vals[3] ? vals[3] : "";
                  u.avatar_url_192 = vals[4] ? vals[4] : "";
                  u.status_emoji = vals[5] ? vals[5] : "";
                  u.status_text = vals[6] ? vals[6] : "";
                  u.is_bot = vals[7] && std::atoi(vals[7]);
                  users_[u.id] = u;
              });

    LOG_DEBUG("loaded " + std::to_string(users_.size()) + " users from DB");
}

} // namespace conduit::cache
