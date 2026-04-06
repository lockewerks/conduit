#pragma once
#include "slack/Types.h"
#include <nlohmann/json.hpp>
#include <string>

namespace conduit::slack {

// takes raw socket mode payloads and turns them into SlackEvents
// this is the translation layer between "whatever slack sends us" and
// "what our UI actually cares about"
class EventDispatcher {
public:
    explicit EventDispatcher(const UserId& self_user_id);

    // parse a socket mode payload into a SlackEvent
    // returns nullopt if it's something we don't care about
    std::optional<SlackEvent> dispatch(const nlohmann::json& payload);

    void setSelfUserId(const UserId& id) { self_user_id_ = id; }

private:
    UserId self_user_id_;

    std::optional<SlackEvent> handleMessage(const nlohmann::json& event);
    std::optional<SlackEvent> handleReaction(const nlohmann::json& event, bool added);
    std::optional<SlackEvent> handleChannelEvent(const nlohmann::json& event);
    std::optional<SlackEvent> handleMemberEvent(const nlohmann::json& event, bool joined);
    std::optional<SlackEvent> handlePresence(const nlohmann::json& event);
    std::optional<SlackEvent> handleTyping(const nlohmann::json& event);
    std::optional<SlackEvent> handleUserChange(const nlohmann::json& event);
    std::optional<SlackEvent> handlePin(const nlohmann::json& event, bool added);
    std::optional<SlackEvent> handleBookmark(const nlohmann::json& event, bool added);
    std::optional<SlackEvent> handleDnd(const nlohmann::json& event);
    std::optional<SlackEvent> handleReminder(const nlohmann::json& event);
    std::optional<SlackEvent> handleStar(const nlohmann::json& event, bool added);
    std::optional<SlackEvent> handleUserGroupUpdate(const nlohmann::json& event);
};

} // namespace conduit::slack
