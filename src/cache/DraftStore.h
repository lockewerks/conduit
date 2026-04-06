#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <string>
#include <unordered_map>
#include <mutex>

namespace conduit::cache {

struct Draft {
    std::string text;
    slack::Timestamp thread_ts;
};

class DraftStore {
public:
    explicit DraftStore(Database& db);

    void loadFromDB();
    void save(const slack::ChannelId& channel, const std::string& text,
              const slack::Timestamp& thread_ts = "");
    Draft get(const slack::ChannelId& channel) const;
    void remove(const slack::ChannelId& channel);
    bool has(const slack::ChannelId& channel) const;

private:
    Database& db_;
    mutable std::mutex mutex_;
    std::unordered_map<slack::ChannelId, Draft> drafts_;
};

} // namespace conduit::cache
