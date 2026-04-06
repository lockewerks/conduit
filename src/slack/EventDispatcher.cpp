#include "slack/EventDispatcher.h"
#include "util/Logger.h"

namespace conduit::slack {

EventDispatcher::EventDispatcher(const UserId& self_user_id)
    : self_user_id_(self_user_id) {}

std::optional<SlackEvent> EventDispatcher::dispatch(const nlohmann::json& payload) {
    if (!payload.contains("event")) {
        return std::nullopt;
    }

    auto& event = payload["event"];
    std::string type = event.value("type", "");

    if (type == "message") {
        return handleMessage(event);
    } else if (type == "reaction_added") {
        return handleReaction(event, true);
    } else if (type == "reaction_removed") {
        return handleReaction(event, false);
    } else if (type == "channel_created" || type == "channel_rename" ||
               type == "channel_archive" || type == "channel_unarchive") {
        return handleChannelEvent(event);
    } else if (type == "member_joined_channel") {
        return handleMemberEvent(event, true);
    } else if (type == "member_left_channel") {
        return handleMemberEvent(event, false);
    } else if (type == "presence_change") {
        return handlePresence(event);
    } else if (type == "user_typing") {
        return handleTyping(event);
    } else if (type == "user_change" || type == "team_join") {
        return handleUserChange(event);
    } else if (type == "pin_added") {
        return handlePin(event, true);
    } else if (type == "pin_removed") {
        return handlePin(event, false);
    } else if (type == "bookmark_added" || type == "bookmark_deleted") {
        return handleBookmark(event, type == "bookmark_added");
    } else if (type == "dnd_updated" || type == "dnd_updated_user") {
        return handleDnd(event);
    } else if (type == "reminder_fired") {
        return handleReminder(event);
    } else if (type == "star_added" || type == "star_removed") {
        return handleStar(event, type == "star_added");
    } else if (type == "subteam_updated" || type == "subteam_members_changed") {
        return handleUserGroupUpdate(event);
    }

    LOG_DEBUG("unhandled event type: " + type);
    return std::nullopt;
}

std::optional<SlackEvent> EventDispatcher::handleMessage(const nlohmann::json& event) {
    SlackEvent e;
    e.channel = event.value("channel", "");
    e.user = event.value("user", "");
    e.ts = event.value("ts", "");
    e.thread_ts = event.value("thread_ts", "");

    std::string subtype = event.value("subtype", "");

    if (subtype == "message_changed") {
        e.type = SlackEvent::Type::MessageChanged;
        if (event.contains("message")) {
            e.message = event["message"].get<Message>();
        }
    } else if (subtype == "message_deleted") {
        e.type = SlackEvent::Type::MessageDeleted;
        e.ts = event.value("deleted_ts", "");
    } else {
        e.type = SlackEvent::Type::MessageNew;
        e.message = event.get<Message>();
    }

    return e;
}

std::optional<SlackEvent> EventDispatcher::handleReaction(const nlohmann::json& event,
                                                           bool added) {
    SlackEvent e;
    e.type = added ? SlackEvent::Type::ReactionAdded : SlackEvent::Type::ReactionRemoved;
    e.user = event.value("user", "");
    e.ts = event.contains("item") ? event["item"].value("ts", "") : "";
    e.channel = event.contains("item") ? event["item"].value("channel", "") : "";

    Reaction r;
    r.emoji_name = event.value("reaction", "");
    r.count = 1;
    r.users = {e.user};
    r.user_reacted = (e.user == self_user_id_);
    e.reaction = r;

    return e;
}

std::optional<SlackEvent> EventDispatcher::handleChannelEvent(const nlohmann::json& event) {
    SlackEvent e;
    std::string type = event.value("type", "");

    if (type == "channel_created") {
        e.type = SlackEvent::Type::ChannelCreated;
    } else if (type == "channel_rename") {
        e.type = SlackEvent::Type::ChannelRenamed;
    } else if (type == "channel_unarchive") {
        e.type = SlackEvent::Type::ChannelUnarchived;
    } else {
        e.type = SlackEvent::Type::ChannelArchived;
    }

    if (event.contains("channel") && event["channel"].is_object()) {
        e.channel = event["channel"].value("id", "");
        e.channel_info = event["channel"].get<Channel>();
    } else {
        e.channel = event.value("channel", "");
    }

    return e;
}

std::optional<SlackEvent> EventDispatcher::handleMemberEvent(const nlohmann::json& event,
                                                              bool joined) {
    SlackEvent e;
    e.type = joined ? SlackEvent::Type::MemberJoined : SlackEvent::Type::MemberLeft;
    e.channel = event.value("channel", "");
    e.user = event.value("user", "");
    return e;
}

std::optional<SlackEvent> EventDispatcher::handlePresence(const nlohmann::json& event) {
    SlackEvent e;
    e.type = SlackEvent::Type::PresenceChange;
    e.user = event.value("user", "");
    e.is_online = (event.value("presence", "") == "active");
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleTyping(const nlohmann::json& event) {
    SlackEvent e;
    e.type = SlackEvent::Type::TypingStart;
    e.channel = event.value("channel", "");
    e.user = event.value("user", "");
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleUserChange(const nlohmann::json& event) {
    SlackEvent e;
    e.type = event.value("type", "") == "team_join" ? SlackEvent::Type::TeamJoin
                                                     : SlackEvent::Type::UserChange;
    if (event.contains("user") && event["user"].is_object()) {
        e.user_info = event["user"].get<User>();
        e.user = e.user_info->id;
    }
    return e;
}

std::optional<SlackEvent> EventDispatcher::handlePin(const nlohmann::json& event, bool added) {
    SlackEvent e;
    e.type = added ? SlackEvent::Type::PinAdded : SlackEvent::Type::PinRemoved;
    e.user = event.value("user", "");
    if (event.contains("item")) {
        auto& item = event["item"];
        e.channel = item.value("channel", "");
        if (item.contains("message")) {
            e.ts = item["message"].value("ts", "");
        }
    }
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleBookmark(const nlohmann::json& event,
                                                          bool added) {
    SlackEvent e;
    e.type = added ? SlackEvent::Type::BookmarkAdded : SlackEvent::Type::BookmarkRemoved;
    if (event.contains("bookmark")) {
        e.bookmark = event["bookmark"].get<Bookmark>();
    }
    e.channel = event.value("channel_id", "");
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleDnd(const nlohmann::json& event) {
    SlackEvent e;
    e.type = SlackEvent::Type::DndUpdated;
    if (event.contains("dnd_status")) {
        e.dnd_status = event["dnd_status"].get<DndStatus>();
    }
    e.user = event.value("user", "");
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleReminder(const nlohmann::json& event) {
    SlackEvent e;
    e.type = SlackEvent::Type::ReminderFired;
    if (event.contains("reminder")) {
        e.reminder = event["reminder"].get<Reminder>();
    }
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleStar(const nlohmann::json& event, bool added) {
    SlackEvent e;
    e.type = added ? SlackEvent::Type::StarAdded : SlackEvent::Type::StarRemoved;
    if (event.contains("item")) {
        e.channel = event["item"].value("channel", "");
        if (event["item"].contains("message")) {
            e.ts = event["item"]["message"].value("ts", "");
        }
    }
    e.user = event.value("user", "");
    return e;
}

std::optional<SlackEvent> EventDispatcher::handleUserGroupUpdate(const nlohmann::json& event) {
    SlackEvent e;
    e.type = SlackEvent::Type::UserGroupUpdated;
    if (event.contains("subteam")) {
        e.user_group = event["subteam"].get<UserGroup>();
    }
    return e;
}

} // namespace conduit::slack
