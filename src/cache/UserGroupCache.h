#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace conduit::cache {

class UserGroupCache {
public:
    explicit UserGroupCache(Database& db);

    void loadFromDB();
    void loadFromAPI(const std::vector<slack::UserGroup>& groups);
    void upsert(const slack::UserGroup& group);
    std::optional<slack::UserGroup> get(const std::string& id) const;
    std::optional<slack::UserGroup> getByHandle(const std::string& handle) const;
    std::vector<slack::UserGroup> getAll() const;
    void flush();

private:
    Database& db_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, slack::UserGroup> groups_;
};

} // namespace conduit::cache
