#pragma once
#include "slack/Types.h"
#include "cache/Database.h"
#include <vector>
#include <mutex>

namespace conduit::cache {

class ReminderCache {
public:
    explicit ReminderCache(Database& db);

    void loadFromDB();
    void loadFromAPI(const std::vector<slack::Reminder>& reminders);
    void add(const slack::Reminder& reminder);
    void remove(const std::string& reminder_id);
    void markComplete(const std::string& reminder_id);
    std::vector<slack::Reminder> getActive() const;
    std::vector<slack::Reminder> getAll() const;
    void flush();

private:
    Database& db_;
    mutable std::mutex mutex_;
    std::vector<slack::Reminder> reminders_;
};

} // namespace conduit::cache
