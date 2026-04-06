#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <vector>
#include <mutex>

namespace conduit::cache {

class SavedItemCache {
public:
    explicit SavedItemCache(Database& db);

    void loadFromDB();
    void loadFromAPI(const std::vector<slack::SavedItem>& items);
    void add(const slack::SavedItem& item);
    void remove(const slack::ChannelId& channel, const slack::Timestamp& ts);
    bool isSaved(const slack::ChannelId& channel, const slack::Timestamp& ts) const;
    std::vector<slack::SavedItem> getAll() const;
    void flush();

private:
    Database& db_;
    mutable std::mutex mutex_;
    std::vector<slack::SavedItem> items_;
};

} // namespace conduit::cache
