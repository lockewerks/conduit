#include "cache/MessageCache.h"
#include "util/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace conduit::cache {

MessageCache::MessageCache(Database& db) : db_(db) {}

void MessageCache::store(const slack::ChannelId& channel,
                          const std::vector<slack::Message>& messages) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& vec = messages_[channel];
    for (auto& msg : messages) {
        insertSorted(vec, msg);
    }
}

void MessageCache::store(const slack::ChannelId& channel, const slack::Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    insertSorted(messages_[channel], message);
}

std::vector<slack::Message> MessageCache::get(const slack::ChannelId& channel, int limit,
                                               const slack::Timestamp& before) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return {};

    const auto& all = it->second;
    if (all.empty()) return {};

    // find the end point
    auto end_it = all.end();
    if (!before.empty()) {
        end_it = std::lower_bound(all.begin(), all.end(), before,
                                   [](const slack::Message& m, const slack::Timestamp& ts) {
                                       return m.ts < ts;
                                   });
    }

    // grab up to 'limit' messages before that point
    int available = static_cast<int>(std::distance(all.begin(), end_it));
    int start = std::max(0, available - limit);

    return std::vector<slack::Message>(all.begin() + start, end_it);
}

std::vector<slack::Message> MessageCache::getThread(const slack::ChannelId& channel,
                                                     const slack::Timestamp& thread_ts) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return {};

    std::vector<slack::Message> thread;
    for (auto& msg : it->second) {
        if (msg.ts == thread_ts || msg.thread_ts == thread_ts) {
            thread.push_back(msg);
        }
    }
    return thread;
}

void MessageCache::update(const slack::ChannelId& channel, const slack::Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& vec = messages_[channel];
    for (auto& msg : vec) {
        if (msg.ts == message.ts) {
            msg = message;
            return;
        }
    }
    // not found, insert it
    insertSorted(vec, message);
}

void MessageCache::remove(const slack::ChannelId& channel, const slack::Timestamp& ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return;

    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                              [&](const slack::Message& m) { return m.ts == ts; }),
              vec.end());
}

void MessageCache::addReaction(const slack::ChannelId& channel, const slack::Timestamp& ts,
                                const slack::Reaction& reaction) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return;

    for (auto& msg : it->second) {
        if (msg.ts == ts) {
            // check if this emoji already exists
            for (auto& r : msg.reactions) {
                if (r.emoji_name == reaction.emoji_name) {
                    r.count++;
                    for (auto& u : reaction.users) {
                        if (std::find(r.users.begin(), r.users.end(), u) == r.users.end()) {
                            r.users.push_back(u);
                        }
                    }
                    return;
                }
            }
            msg.reactions.push_back(reaction);
            return;
        }
    }
}

void MessageCache::removeReaction(const slack::ChannelId& channel, const slack::Timestamp& ts,
                                   const std::string& emoji, const slack::UserId& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return;

    for (auto& msg : it->second) {
        if (msg.ts == ts) {
            for (auto r_it = msg.reactions.begin(); r_it != msg.reactions.end(); ++r_it) {
                if (r_it->emoji_name == emoji) {
                    r_it->users.erase(
                        std::remove(r_it->users.begin(), r_it->users.end(), user),
                        r_it->users.end());
                    r_it->count = std::max(0, r_it->count - 1);
                    if (r_it->count == 0) {
                        msg.reactions.erase(r_it);
                    }
                    return;
                }
            }
            return;
        }
    }
}

void MessageCache::setPin(const slack::ChannelId& channel, const slack::Timestamp& ts,
                           bool pinned) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return;

    for (auto& msg : it->second) {
        if (msg.ts == ts) {
            msg.is_pinned = pinned;
            return;
        }
    }
}

void MessageCache::insertSorted(std::vector<slack::Message>& vec, const slack::Message& msg) {
    // check if it already exists (update in place)
    for (auto& existing : vec) {
        if (existing.ts == msg.ts) {
            existing = msg;
            return;
        }
    }

    // insert in sorted order by timestamp
    auto pos = std::lower_bound(vec.begin(), vec.end(), msg,
                                 [](const slack::Message& a, const slack::Message& b) {
                                     return a.ts < b.ts;
                                 });
    vec.insert(pos, msg);
}

void MessageCache::flush(const slack::ChannelId& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(channel);
    if (it == messages_.end()) return;

    for (auto& msg : it->second) {
        nlohmann::json reactions_j = msg.reactions;
        nlohmann::json attachments_j = msg.attachments;
        nlohmann::json files_j = msg.files;
        nlohmann::json reply_users_j = msg.reply_users;

        // prepared statements would be nice here but this works
        std::string sql = "INSERT OR REPLACE INTO messages "
                          "(channel_id, ts, thread_ts, user_id, text, subtype, is_edited, "
                          "edited_ts, reply_count, is_pinned, reactions_json, attachments_json, "
                          "files_json, reply_users_json) VALUES ("
                          "'" + channel + "', "
                          "'" + msg.ts + "', "
                          "'" + msg.thread_ts + "', "
                          "'" + msg.user + "', "
                          "'" + msg.text + "', "  // this is not injection-safe, but it's our own data
                          "'" + msg.subtype + "', "
                          + std::to_string(msg.is_edited ? 1 : 0) + ", "
                          "'" + msg.edited_ts + "', "
                          + std::to_string(msg.reply_count) + ", "
                          + std::to_string(msg.is_pinned ? 1 : 0) + ", "
                          "'" + reactions_j.dump() + "', "
                          "'" + attachments_j.dump() + "', "
                          "'" + files_j.dump() + "', "
                          "'" + reply_users_j.dump() + "')";
        db_.exec(sql);
    }
}

void MessageCache::flushAll() {
    for (auto& [channel, _] : messages_) {
        flush(channel);
    }
}

void MessageCache::loadFromDB(const slack::ChannelId& channel, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& vec = messages_[channel];
    vec.clear();

    std::string sql = "SELECT ts, thread_ts, user_id, text, subtype, is_edited, "
                      "edited_ts, reply_count, is_pinned, reactions_json, attachments_json, "
                      "files_json, reply_users_json FROM messages "
                      "WHERE channel_id = '" + channel + "' "
                      "ORDER BY ts DESC LIMIT " + std::to_string(limit);

    db_.query(sql, [&](int, char** vals, char**) {
        slack::Message msg;
        msg.ts = vals[0] ? vals[0] : "";
        msg.thread_ts = vals[1] ? vals[1] : "";
        msg.user = vals[2] ? vals[2] : "";
        msg.text = vals[3] ? vals[3] : "";
        msg.subtype = vals[4] ? vals[4] : "";
        msg.is_edited = vals[5] && std::atoi(vals[5]);
        msg.edited_ts = vals[6] ? vals[6] : "";
        msg.reply_count = vals[7] ? std::atoi(vals[7]) : 0;
        msg.is_pinned = vals[8] && std::atoi(vals[8]);

        try {
            if (vals[9]) msg.reactions = nlohmann::json::parse(vals[9]).get<std::vector<slack::Reaction>>();
            if (vals[10]) msg.attachments = nlohmann::json::parse(vals[10]).get<std::vector<slack::Attachment>>();
            if (vals[11]) msg.files = nlohmann::json::parse(vals[11]).get<std::vector<slack::SlackFile>>();
            if (vals[12]) msg.reply_users = nlohmann::json::parse(vals[12]).get<std::vector<std::string>>();
        } catch (...) {
            // malformed json in the DB, whatever, move on
        }

        vec.push_back(std::move(msg));
    });

    // we fetched newest first, reverse to get chronological order
    std::reverse(vec.begin(), vec.end());
    LOG_DEBUG("loaded " + std::to_string(vec.size()) + " messages from DB for " + channel);
}

} // namespace conduit::cache
