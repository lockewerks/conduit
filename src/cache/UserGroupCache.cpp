#include "cache/UserGroupCache.h"
#include "util/Logger.h"
#include <nlohmann/json.hpp>

namespace conduit::cache {

UserGroupCache::UserGroupCache(Database& db) : db_(db) {}

void UserGroupCache::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    groups_.clear();

    db_.query("SELECT id, handle, name, description, members_json, member_count FROM user_groups",
              [&](int, char** vals, char**) {
                  slack::UserGroup g;
                  g.id = vals[0] ? vals[0] : "";
                  g.handle = vals[1] ? vals[1] : "";
                  g.name = vals[2] ? vals[2] : "";
                  g.description = vals[3] ? vals[3] : "";
                  g.member_count = vals[5] ? std::atoi(vals[5]) : 0;
                  if (vals[4]) {
                      try {
                          auto members = nlohmann::json::parse(vals[4]);
                          if (members.is_array()) {
                              g.members = members.get<std::vector<std::string>>();
                          }
                      } catch (...) {}
                  }
                  groups_[g.id] = g;
              });

    LOG_DEBUG("loaded " + std::to_string(groups_.size()) + " user groups from DB");
}

void UserGroupCache::loadFromAPI(const std::vector<slack::UserGroup>& groups) {
    std::lock_guard<std::mutex> lock(mutex_);
    groups_.clear();
    for (auto& g : groups) {
        groups_[g.id] = g;
    }
    flush();
    LOG_INFO("loaded " + std::to_string(groups.size()) + " user groups from API");
}

void UserGroupCache::upsert(const slack::UserGroup& group) {
    std::lock_guard<std::mutex> lock(mutex_);
    groups_[group.id] = group;
}

std::optional<slack::UserGroup> UserGroupCache::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = groups_.find(id);
    if (it != groups_.end()) return it->second;
    return std::nullopt;
}

std::optional<slack::UserGroup> UserGroupCache::getByHandle(const std::string& handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, g] : groups_) {
        if (g.handle == handle) return g;
    }
    return std::nullopt;
}

std::vector<slack::UserGroup> UserGroupCache::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<slack::UserGroup> result;
    for (auto& [_, g] : groups_) {
        result.push_back(g);
    }
    return result;
}

void UserGroupCache::flush() {
    db_.exec("DELETE FROM user_groups");
    for (auto& [_, g] : groups_) {
        nlohmann::json members_j = g.members;
        std::string sql = "INSERT INTO user_groups "
                          "(id, handle, name, description, members_json, member_count) VALUES ("
                          "'" + g.id + "', '" + g.handle + "', '" + g.name + "', "
                          "'" + g.description + "', '" + members_j.dump() + "', "
                          + std::to_string(g.member_count) + ")";
        db_.exec(sql);
    }
}

} // namespace conduit::cache
