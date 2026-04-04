#pragma once
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace conduit::slack {

// slack uses strings for timestamps because of course it does
using Timestamp = std::string;
using UserId = std::string;
using ChannelId = std::string;
using TeamId = std::string;
using FileId = std::string;

enum class ChannelType {
    PublicChannel,
    PrivateChannel,
    DirectMessage,
    MultiPartyDM,
    GroupDM
};

struct User {
    UserId id;
    std::string display_name;
    std::string real_name;
    std::string avatar_url_72;
    std::string avatar_url_192;
    std::string status_emoji;
    std::string status_text;
    bool is_bot = false;
    bool is_online = false;

    // the display name we actually show in the UI
    std::string effectiveName() const {
        if (!display_name.empty()) return display_name;
        if (!real_name.empty()) return real_name;
        return id; // shrug
    }
};

// json conversion - we do this manually because slack's API is... inconsistent
inline void from_json(const nlohmann::json& j, User& u) {
    u.id = j.value("id", "");
    if (j.contains("profile")) {
        auto& p = j["profile"];
        u.display_name = p.value("display_name", "");
        u.real_name = p.value("real_name", "");
        u.avatar_url_72 = p.value("image_72", "");
        u.avatar_url_192 = p.value("image_192", "");
        u.status_emoji = p.value("status_emoji", "");
        u.status_text = p.value("status_text", "");
    }
    // sometimes display_name is at the top level, thanks slack
    if (u.display_name.empty()) u.display_name = j.value("display_name", "");
    if (u.real_name.empty()) u.real_name = j.value("real_name", "");
    u.is_bot = j.value("is_bot", false);
}

struct Reaction {
    std::string emoji_name;
    int count = 0;
    std::vector<UserId> users;
    bool user_reacted = false;
};

inline void from_json(const nlohmann::json& j, Reaction& r) {
    r.emoji_name = j.value("name", "");
    r.count = j.value("count", 0);
    if (j.contains("users") && j["users"].is_array()) {
        r.users = j["users"].get<std::vector<std::string>>();
    }
}

inline void to_json(nlohmann::json& j, const Reaction& r) {
    j = {{"name", r.emoji_name}, {"count", r.count}, {"users", r.users}};
}

struct Attachment {
    std::string fallback;
    std::string color;
    std::string title;
    std::string title_link;
    std::string text;
    std::string image_url;
    int image_width = 0;
    int image_height = 0;
};

inline void from_json(const nlohmann::json& j, Attachment& a) {
    a.fallback = j.value("fallback", "");
    a.color = j.value("color", "");
    a.title = j.value("title", "");
    a.title_link = j.value("title_link", "");
    a.text = j.value("text", "");
    a.image_url = j.value("image_url", "");
    a.image_width = j.value("image_width", 0);
    a.image_height = j.value("image_height", 0);
}

inline void to_json(nlohmann::json& j, const Attachment& a) {
    j = {{"fallback", a.fallback}, {"color", a.color}, {"title", a.title},
         {"title_link", a.title_link}, {"text", a.text}, {"image_url", a.image_url},
         {"image_width", a.image_width}, {"image_height", a.image_height}};
}

struct SlackFile {
    FileId id;
    std::string name;
    std::string mimetype;
    std::string url_private;
    std::string thumb_360;
    std::string thumb_480;
    int size = 0;
    int original_w = 0;
    int original_h = 0;
    std::string permalink;
};

inline void from_json(const nlohmann::json& j, SlackFile& f) {
    f.id = j.value("id", "");
    f.name = j.value("name", "");
    f.mimetype = j.value("mimetype", "");
    f.url_private = j.value("url_private", "");
    f.thumb_360 = j.value("thumb_360", "");
    f.thumb_480 = j.value("thumb_480", "");
    f.size = j.value("size", 0);
    f.original_w = j.value("original_w", 0);
    f.original_h = j.value("original_h", 0);
    f.permalink = j.value("permalink", "");
}

inline void to_json(nlohmann::json& j, const SlackFile& f) {
    j = {{"id", f.id}, {"name", f.name}, {"mimetype", f.mimetype},
         {"url_private", f.url_private}, {"thumb_360", f.thumb_360},
         {"size", f.size}, {"original_w", f.original_w}, {"original_h", f.original_h}};
}

struct Message {
    Timestamp ts;
    Timestamp thread_ts;
    UserId user;
    std::string text;
    std::string subtype;
    std::vector<Reaction> reactions;
    std::vector<Attachment> attachments;
    std::vector<SlackFile> files;
    bool is_edited = false;
    Timestamp edited_ts;
    int reply_count = 0;
    std::vector<UserId> reply_users;
    bool is_pinned = false;

    // computed at render time, don't persist this
    mutable float rendered_height = 0.0f;
};

inline void from_json(const nlohmann::json& j, Message& m) {
    m.ts = j.value("ts", "");
    m.thread_ts = j.value("thread_ts", "");
    m.user = j.value("user", "");
    m.text = j.value("text", "");
    m.subtype = j.value("subtype", "");
    m.reply_count = j.value("reply_count", 0);
    m.is_pinned = j.value("is_pinned", false);

    if (j.contains("edited")) {
        m.is_edited = true;
        m.edited_ts = j["edited"].value("ts", "");
    }
    if (j.contains("reactions") && j["reactions"].is_array()) {
        m.reactions = j["reactions"].get<std::vector<Reaction>>();
    }
    if (j.contains("attachments") && j["attachments"].is_array()) {
        m.attachments = j["attachments"].get<std::vector<Attachment>>();
    }
    if (j.contains("files") && j["files"].is_array()) {
        m.files = j["files"].get<std::vector<SlackFile>>();
    }
    if (j.contains("reply_users") && j["reply_users"].is_array()) {
        m.reply_users = j["reply_users"].get<std::vector<std::string>>();
    }
}

struct Channel {
    ChannelId id;
    std::string name;
    std::string topic;
    std::string purpose;
    ChannelType type = ChannelType::PublicChannel;
    bool is_member = false;
    bool is_muted = false;
    bool is_archived = false;
    int unread_count = 0;
    bool has_unreads = false;
    Timestamp last_read;
    int member_count = 0;
    UserId dm_user_id;

    // not persisted
    bool is_open_in_buffer = false;
};

inline void from_json(const nlohmann::json& j, Channel& c) {
    c.id = j.value("id", "");
    c.name = j.value("name", "");
    c.is_member = j.value("is_member", false);
    c.is_archived = j.value("is_archived", false);
    c.member_count = j.value("num_members", 0);

    if (j.contains("topic") && j["topic"].is_object()) {
        c.topic = j["topic"].value("value", "");
    }
    if (j.contains("purpose") && j["purpose"].is_object()) {
        c.purpose = j["purpose"].value("value", "");
    }

    // figure out what kind of channel this is
    // slack's naming conventions: C = channel, D = DM, G = group/mpdm
    if (j.value("is_im", false)) {
        c.type = ChannelType::DirectMessage;
        c.dm_user_id = j.value("user", "");
    } else if (j.value("is_mpim", false)) {
        c.type = ChannelType::MultiPartyDM;
    } else if (j.value("is_group", false) || j.value("is_private", false)) {
        c.type = ChannelType::PrivateChannel;
    } else {
        c.type = ChannelType::PublicChannel;
    }

    c.is_muted = false; // slack doesn't always include this, we track it ourselves
}

struct ThreadInfo {
    Timestamp parent_ts;
    ChannelId channel_id;
    int reply_count = 0;
    std::vector<UserId> participants;
    Timestamp latest_reply;
};

struct TypingEvent {
    ChannelId channel;
    UserId user;
    std::chrono::steady_clock::time_point expires;
};

// the big event union - everything that comes in from socket mode
struct SlackEvent {
    enum class Type {
        MessageNew,
        MessageChanged,
        MessageDeleted,
        ReactionAdded,
        ReactionRemoved,
        ChannelMarked,
        ChannelJoined,
        ChannelLeft,
        ChannelCreated,
        ChannelRenamed,
        ChannelArchived,
        MemberJoined,
        MemberLeft,
        PresenceChange,
        TypingStart,
        FileShared,
        PinAdded,
        PinRemoved,
        UserChange,
        TeamJoin,
        ConnectionStatus,
    };

    Type type;
    ChannelId channel;
    UserId user;
    Timestamp ts;
    Timestamp thread_ts;

    std::optional<Message> message;
    std::optional<Reaction> reaction;
    std::optional<Channel> channel_info;
    std::optional<User> user_info;
    std::optional<bool> is_online;
    std::optional<std::string> connection_state;
};

// thread-safe queue for passing events between threads
// nothing fancy, just a mutex and a vector because premature optimization etc
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
    }

    bool pop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.erase(queue_.begin());
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<T> queue_;
};

} // namespace conduit::slack
