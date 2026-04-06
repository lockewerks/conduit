#include "cache/ReminderCache.h"
#include "util/Logger.h"

namespace conduit::cache {

ReminderCache::ReminderCache(Database& db) : db_(db) {}

void ReminderCache::loadFromDB() {
    std::lock_guard<std::mutex> lock(mutex_);
    reminders_.clear();

    db_.query("SELECT id, creator, text, user_id, time, complete_ts, is_complete FROM reminders",
              [&](int, char** vals, char**) {
                  slack::Reminder r;
                  r.id = vals[0] ? vals[0] : "";
                  r.creator = vals[1] ? vals[1] : "";
                  r.text = vals[2] ? vals[2] : "";
                  r.user = vals[3] ? vals[3] : "";
                  r.time = vals[4] ? std::atoll(vals[4]) : 0;
                  r.complete_ts = vals[5] ? std::atoll(vals[5]) : 0;
                  r.is_complete = vals[6] && std::atoi(vals[6]);
                  reminders_.push_back(r);
              });

    LOG_DEBUG("loaded " + std::to_string(reminders_.size()) + " reminders from DB");
}

void ReminderCache::loadFromAPI(const std::vector<slack::Reminder>& reminders) {
    std::lock_guard<std::mutex> lock(mutex_);
    reminders_ = reminders;
    flush();
}

void ReminderCache::add(const slack::Reminder& reminder) {
    std::lock_guard<std::mutex> lock(mutex_);
    reminders_.push_back(reminder);
}

void ReminderCache::remove(const std::string& reminder_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    reminders_.erase(
        std::remove_if(reminders_.begin(), reminders_.end(),
                       [&](const slack::Reminder& r) { return r.id == reminder_id; }),
        reminders_.end());
    db_.exec("DELETE FROM reminders WHERE id = '" + reminder_id + "'");
}

void ReminderCache::markComplete(const std::string& reminder_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& r : reminders_) {
        if (r.id == reminder_id) {
            r.is_complete = true;
            break;
        }
    }
}

std::vector<slack::Reminder> ReminderCache::getActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<slack::Reminder> result;
    for (auto& r : reminders_) {
        if (!r.is_complete) result.push_back(r);
    }
    return result;
}

std::vector<slack::Reminder> ReminderCache::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return reminders_;
}

void ReminderCache::flush() {
    db_.exec("DELETE FROM reminders");
    for (auto& r : reminders_) {
        std::string sql = "INSERT INTO reminders "
                          "(id, creator, text, user_id, time, complete_ts, is_complete) VALUES ("
                          "'" + r.id + "', '" + r.creator + "', '" + r.text + "', "
                          "'" + r.user + "', " + std::to_string(r.time) + ", "
                          + std::to_string(r.complete_ts) + ", "
                          + std::to_string(r.is_complete ? 1 : 0) + ")";
        db_.exec(sql);
    }
}

} // namespace conduit::cache
