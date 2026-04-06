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

    // extended profile
    std::string title;
    std::string email;
    std::string phone;
    std::string tz;
    std::string tz_label;
    int tz_offset = 0;
    std::string pronouns;
    bool is_admin = false;
    bool is_owner = false;

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

    // extended profile fields
    if (j.contains("profile")) {
        auto& p = j["profile"];
        u.title = p.value("title", "");
        u.email = p.value("email", "");
        u.phone = p.value("phone", "");
        u.pronouns = p.value("pronouns", "");
    }
    u.tz = j.value("tz", "");
    u.tz_label = j.value("tz_label", "");
    u.tz_offset = j.value("tz_offset", 0);
    u.is_admin = j.value("is_admin", false);
    u.is_owner = j.value("is_owner", false);
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

// -- Block Kit types for modern message rendering --

struct BlockElement {
    std::string type;       // "button", "static_select", "overflow", "image",
                            // "mrkdwn", "plain_text", "rich_text_section", etc.
    std::string text;
    std::string action_id;
    std::string url;
    std::string value;
    std::string image_url;
    int image_width = 0;
    int image_height = 0;
    std::string alt_text;

    // style flags for rich_text_section elements
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool code = false;

    // for user/channel/usergroup mentions inside rich_text
    std::string user_id;
    std::string channel_id;
    std::string usergroup_id;

    std::vector<BlockElement> elements;     // nested elements
    std::vector<BlockElement> options;       // for selects/overflow
};

inline void from_json(const nlohmann::json& j, BlockElement& e) {
    e.type = j.value("type", "");
    e.action_id = j.value("action_id", "");
    e.url = j.value("url", "");
    e.value = j.value("value", "");
    e.image_url = j.value("image_url", "");
    e.image_width = j.value("image_width", 0);
    e.image_height = j.value("image_height", 0);
    e.alt_text = j.value("alt_text", "");
    e.user_id = j.value("user_id", "");
    e.channel_id = j.value("channel_id", "");
    e.usergroup_id = j.value("usergroup_id", "");

    // text can be a string or an object with "text" field
    if (j.contains("text")) {
        if (j["text"].is_string()) {
            e.text = j["text"].get<std::string>();
        } else if (j["text"].is_object()) {
            e.text = j["text"].value("text", "");
        }
    }

    // style object for rich_text elements
    if (j.contains("style") && j["style"].is_object()) {
        auto& s = j["style"];
        e.bold = s.value("bold", false);
        e.italic = s.value("italic", false);
        e.strike = s.value("strike", false);
        e.code = s.value("code", false);
    }

    if (j.contains("elements") && j["elements"].is_array()) {
        e.elements = j["elements"].get<std::vector<BlockElement>>();
    }
    if (j.contains("options") && j["options"].is_array()) {
        e.options = j["options"].get<std::vector<BlockElement>>();
    }
}

inline void to_json(nlohmann::json& j, const BlockElement& e) {
    j = {{"type", e.type}, {"text", e.text}};
    if (!e.action_id.empty()) j["action_id"] = e.action_id;
    if (!e.url.empty()) j["url"] = e.url;
}

struct Block {
    std::string type;       // "rich_text", "section", "divider", "context",
                            // "actions", "image", "header"
    std::string block_id;
    std::optional<BlockElement> text;
    std::optional<BlockElement> accessory;
    std::vector<BlockElement> elements;
    std::vector<BlockElement> fields;
    std::string image_url;
    std::string alt_text;
    std::string title_text;
};

inline void from_json(const nlohmann::json& j, Block& b) {
    b.type = j.value("type", "");
    b.block_id = j.value("block_id", "");
    b.image_url = j.value("image_url", "");
    b.alt_text = j.value("alt_text", "");

    if (j.contains("text") && j["text"].is_object()) {
        b.text = j["text"].get<BlockElement>();
    }
    if (j.contains("accessory") && j["accessory"].is_object()) {
        b.accessory = j["accessory"].get<BlockElement>();
    }
    if (j.contains("elements") && j["elements"].is_array()) {
        b.elements = j["elements"].get<std::vector<BlockElement>>();
    }
    if (j.contains("fields") && j["fields"].is_array()) {
        b.fields = j["fields"].get<std::vector<BlockElement>>();
    }
    if (j.contains("title") && j["title"].is_object()) {
        b.title_text = j["title"].value("text", "");
    }
}

inline void to_json(nlohmann::json& j, const Block& b) {
    j = {{"type", b.type}, {"block_id", b.block_id}};
    if (!b.elements.empty()) j["elements"] = b.elements;
}

// -- domain types for new features --

struct Bookmark {
    std::string id;
    ChannelId channel_id;
    std::string title;
    std::string link;
    std::string emoji;
    std::string type;       // "link" or "message"
    UserId created_by;
    int64_t date_created = 0;
};

inline void from_json(const nlohmann::json& j, Bookmark& b) {
    b.id = j.value("id", "");
    b.channel_id = j.value("channel_id", "");
    b.title = j.value("title", "");
    b.link = j.value("link", "");
    b.emoji = j.value("emoji", "");
    b.type = j.value("type", "link");
    b.created_by = j.value("created_by", "");
    b.date_created = j.value("date_created", int64_t(0));
}

inline void to_json(nlohmann::json& j, const Bookmark& b) {
    j = {{"id", b.id}, {"channel_id", b.channel_id}, {"title", b.title},
         {"link", b.link}, {"emoji", b.emoji}, {"type", b.type}};
}

struct Reminder {
    std::string id;
    UserId creator;
    std::string text;
    UserId user;
    int64_t time = 0;
    int64_t complete_ts = 0;
    bool is_complete = false;
};

inline void from_json(const nlohmann::json& j, Reminder& r) {
    r.id = j.value("id", "");
    r.creator = j.value("creator", "");
    r.text = j.value("text", "");
    r.user = j.value("user", "");
    r.time = j.value("time", int64_t(0));
    r.complete_ts = j.value("complete_ts", int64_t(0));
    r.is_complete = j.value("complete_ts", int64_t(0)) > 0;
}

inline void to_json(nlohmann::json& j, const Reminder& r) {
    j = {{"id", r.id}, {"text", r.text}, {"time", r.time}, {"is_complete", r.is_complete}};
}

struct ScheduledMessage {
    std::string id;
    ChannelId channel_id;
    std::string text;
    int64_t post_at = 0;
    int64_t date_created = 0;
};

inline void from_json(const nlohmann::json& j, ScheduledMessage& s) {
    s.id = j.value("id", "");
    s.channel_id = j.value("channel_id", "");
    s.text = j.value("text", "");
    s.post_at = j.value("post_at", int64_t(0));
    s.date_created = j.value("date_created", int64_t(0));
}

inline void to_json(nlohmann::json& j, const ScheduledMessage& s) {
    j = {{"id", s.id}, {"channel_id", s.channel_id}, {"text", s.text}, {"post_at", s.post_at}};
}

struct UserGroup {
    std::string id;
    std::string handle;
    std::string name;
    std::string description;
    std::vector<UserId> members;
    int member_count = 0;
};

inline void from_json(const nlohmann::json& j, UserGroup& g) {
    g.id = j.value("id", "");
    g.handle = j.value("handle", "");
    g.name = j.value("name", "");
    g.description = j.value("description", "");
    g.member_count = j.value("user_count", 0);
    if (j.contains("users") && j["users"].is_array()) {
        g.members = j["users"].get<std::vector<std::string>>();
        if (g.member_count == 0) g.member_count = static_cast<int>(g.members.size());
    }
}

inline void to_json(nlohmann::json& j, const UserGroup& g) {
    j = {{"id", g.id}, {"handle", g.handle}, {"name", g.name}, {"users", g.members}};
}

struct SavedItem {
    std::string type;       // "message"
    ChannelId channel_id;
    Timestamp message_ts;
    int64_t date_saved = 0;
};

struct DndStatus {
    bool dnd_enabled = false;
    int64_t next_dnd_start = 0;
    int64_t next_dnd_end = 0;
    bool snooze_enabled = false;
    int64_t snooze_endtime = 0;
};

inline void from_json(const nlohmann::json& j, DndStatus& d) {
    d.dnd_enabled = j.value("dnd_enabled", false);
    d.next_dnd_start = j.value("next_dnd_start_ts", int64_t(0));
    d.next_dnd_end = j.value("next_dnd_end_ts", int64_t(0));
    d.snooze_enabled = j.value("snooze_enabled", false);
    d.snooze_endtime = j.value("snooze_endtime", int64_t(0));
}

struct ChannelSection {
    std::string id;
    std::string name;
    std::vector<ChannelId> channel_ids;
    int sort_order = 0;
    bool collapsed = false;
};

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
    std::vector<Block> blocks;
    std::vector<SlackFile> files;
    bool is_edited = false;
    Timestamp edited_ts;
    int reply_count = 0;
    std::vector<UserId> reply_users;
    bool is_pinned = false;
    bool reply_broadcast = false;
    std::string bot_id;

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
    m.reply_broadcast = j.value("reply_broadcast", false);
    m.bot_id = j.value("bot_id", "");

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
    if (j.contains("blocks") && j["blocks"].is_array()) {
        m.blocks = j["blocks"].get<std::vector<Block>>();
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
        // IMs don't have is_member — they're open if is_open is true or if
        // they exist in the response at all. treat them as joined.
        if (!c.is_member) {
            c.is_member = j.value("is_open", true);
        }
    } else if (j.value("is_mpim", false)) {
        c.type = ChannelType::MultiPartyDM;
        // same for mpdm — is_member may not be set
        if (!c.is_member) {
            c.is_member = j.value("is_open", true);
        }
    } else if (j.value("is_group", false) || j.value("is_private", false)) {
        c.type = ChannelType::PrivateChannel;
    } else {
        c.type = ChannelType::PublicChannel;
    }

    c.is_muted = false;
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
        ChannelUnarchived,
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
        BookmarkAdded,
        BookmarkRemoved,
        DndUpdated,
        ReminderFired,
        StarAdded,
        StarRemoved,
        UserGroupUpdated,
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

    // new feature event payloads
    std::optional<Bookmark> bookmark;
    std::optional<DndStatus> dnd_status;
    std::optional<Reminder> reminder;
    std::optional<UserGroup> user_group;
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
