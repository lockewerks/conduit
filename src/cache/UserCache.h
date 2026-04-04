#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <unordered_map>
#include <mutex>

namespace conduit::cache {

class UserCache {
public:
    explicit UserCache(Database& db);

    void loadFromAPI(const std::vector<slack::User>& users);
    void upsert(const slack::User& user);
    std::optional<slack::User> get(const slack::UserId& id) const;
    std::vector<slack::User> getAll() const;

    // resolve a user ID to a display name (with fallback)
    std::string displayName(const slack::UserId& id) const;

    // presence tracking
    void setOnline(const slack::UserId& id, bool online);
    bool isOnline(const slack::UserId& id) const;

    void flush();
    void loadFromDB();

private:
    Database& db_;
    mutable std::mutex mutex_;
    std::unordered_map<slack::UserId, slack::User> users_;
};

} // namespace conduit::cache
