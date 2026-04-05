#include "slack/SlackClient.h"
#include "util/Logger.h"

namespace conduit::slack {

SlackClient::SlackClient(const OrgConfig& config, cache::Database& db)
    : config_(config), db_(db), channel_cache_(db), user_cache_(db) {}

SlackClient::~SlackClient() {
    disconnect();
}

bool SlackClient::connect() {
    LOG_INFO("connecting to " + config_.name + "...");

    // set up the REST API client
    api_ = std::make_unique<WebAPI>(config_.user_token);

    // validate the token
    auth_info_ = SlackAuth::test(*api_);
    if (!auth_info_.ok) {
        LOG_ERROR("auth failed for " + config_.name + " - check your token");
        return false;
    }

    // set up the event dispatcher
    dispatcher_ = std::make_unique<EventDispatcher>(auth_info_.user_id);

    // load cached data from sqlite first for fast startup
    channel_cache_.loadFromDB();
    user_cache_.loadFromDB();

    // then fetch fresh data from the API
    bootstrapData();

    // start socket mode if we have an app token
    if (!config_.app_token.empty()) {
        socket_ = std::make_unique<SocketModeClient>(config_.app_token);
        socket_->setEventCallback([this](const nlohmann::json& payload) {
            auto event = dispatcher_->dispatch(payload);
            if (event) {
                event_queue_.push(std::move(*event));
            }
        });

        if (!socket_->connect()) {
            LOG_WARN("socket mode failed to connect - real-time events won't work");
            // don't fail the whole connection, REST still works
        }
    } else {
        LOG_WARN("no app token for " + config_.name + " - no real-time events");
    }

    // post a connection event
    SlackEvent conn_event;
    conn_event.type = SlackEvent::Type::ConnectionStatus;
    conn_event.connection_state = "connected";
    event_queue_.push(conn_event);

    return true;
}

void SlackClient::disconnect() {
    if (socket_) {
        socket_->disconnect();
        socket_.reset();
    }
    api_.reset();
    dispatcher_.reset();

    SlackEvent disc_event;
    disc_event.type = SlackEvent::Type::ConnectionStatus;
    disc_event.connection_state = "disconnected";
    event_queue_.push(disc_event);
}

bool SlackClient::isConnected() const {
    return api_ != nullptr && auth_info_.ok;
}

void SlackClient::bootstrapData() {
    // fetch channels (paginated)
    auto channels = fetchPaginated<Channel>("conversations.list", "channels",
                                             "types=public_channel,private_channel,im,mpim&exclude_archived=false");
    channel_cache_.loadFromAPI(channels);

    // fetch users (paginated)
    auto users = fetchPaginated<User>("users.list", "members");
    user_cache_.loadFromAPI(users);

    LOG_INFO("bootstrap complete: " + std::to_string(channels.size()) + " channels, " +
             std::to_string(users.size()) + " users");
}

template <typename T>
std::vector<T> SlackClient::fetchPaginated(const std::string& method,
                                            const std::string& list_key,
                                            const std::string& extra_params) {
    std::vector<T> all;
    std::string cursor;

    do {
        std::string params = extra_params;
        params += "&limit=200";
        if (!cursor.empty()) {
            params += "&cursor=" + cursor;
        }

        rate_limiter_.recordCall(method);
        auto result = api_->get(method, params);
        if (!result || !result->value("ok", false)) break;

        auto& j = *result;
        if (j.contains(list_key) && j[list_key].is_array()) {
            for (auto& item : j[list_key]) {
                try {
                    all.push_back(item.get<T>());
                } catch (const std::exception& e) {
                    LOG_DEBUG(std::string("skipped malformed item: ") + e.what());
                }
            }
        }

        // pagination cursor
        cursor.clear();
        if (j.contains("response_metadata") && j["response_metadata"].contains("next_cursor")) {
            cursor = j["response_metadata"]["next_cursor"].get<std::string>();
        }
    } while (!cursor.empty());

    return all;
}

// channel operations
std::vector<Channel> SlackClient::getChannels(bool include_archived) {
    return channel_cache_.getAll(include_archived);
}

std::optional<Channel> SlackClient::getChannel(const ChannelId& id) {
    return channel_cache_.get(id);
}

bool SlackClient::joinChannel(const ChannelId& id) {
    auto result = api_->post("conversations.join", {{"channel", id}});
    return result && result->value("ok", false);
}

bool SlackClient::leaveChannel(const ChannelId& id) {
    auto result = api_->post("conversations.leave", {{"channel", id}});
    return result && result->value("ok", false);
}

bool SlackClient::markRead(const ChannelId& channel, const Timestamp& ts) {
    auto result = api_->post("conversations.mark", {{"channel", channel}, {"ts", ts}});
    if (result && result->value("ok", false)) {
        channel_cache_.updateLastRead(channel, ts);
        channel_cache_.updateUnreadCount(channel, 0);
        return true;
    }
    return false;
}

bool SlackClient::setTopic(const ChannelId& channel, const std::string& topic) {
    auto result = api_->post("conversations.setTopic", {{"channel", channel}, {"topic", topic}});
    if (result && result->value("ok", false)) {
        channel_cache_.updateTopic(channel, topic);
        return true;
    }
    return false;
}

// message operations
std::vector<Message> SlackClient::getHistory(const ChannelId& channel, int limit,
                                              const std::optional<Timestamp>& before) {
    std::string params = "channel=" + channel + "&limit=" + std::to_string(limit);
    if (before) params += "&latest=" + *before;

    rate_limiter_.recordCall("conversations.history");
    auto result = api_->get("conversations.history", params);
    if (!result || !result->value("ok", false)) return {};

    std::vector<Message> messages;
    if (result->contains("messages") && (*result)["messages"].is_array()) {
        for (auto& m : (*result)["messages"]) {
            try {
                auto msg = m.get<Message>();
                // log file info so we can debug image rendering
                if (!msg.files.empty()) {
                    LOG_DEBUG("msg has " + std::to_string(msg.files.size()) +
                              " files: " + msg.files[0].name + " (" + msg.files[0].mimetype + ")");
                }
                if (m.contains("attachments") && m["attachments"].is_array() && !m["attachments"].empty()) {
                    auto& att = m["attachments"][0];
                    std::string att_type = att.value("service_name", att.value("fallback", "unknown"));
                    LOG_DEBUG("msg has attachment: " + att_type);
                    // slack sends giphy/unfurled images as attachments, not files
                    // pull them into the files array so our renderer sees them
                    for (auto& a : m["attachments"]) {
                        std::string img_url = a.value("image_url", "");
                        if (!img_url.empty()) {
                            SlackFile sf;
                            sf.name = a.value("title", a.value("fallback", "image"));
                            sf.url_private = img_url;
                            sf.thumb_360 = a.value("thumb_url", img_url);
                            sf.mimetype = "image/gif"; // attachments with image_url are usually gifs
                            sf.original_w = a.value("image_width", 0);
                            sf.original_h = a.value("image_height", 0);
                            msg.files.push_back(sf);
                            LOG_DEBUG("promoted attachment image to file: " + img_url.substr(0, 80));
                        }
                    }
                }
                messages.push_back(std::move(msg));
            } catch (...) {}
        }
    }

    // slack returns newest first, we want oldest first
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<Message> SlackClient::getThreadReplies(const ChannelId& channel,
                                                    const Timestamp& thread_ts) {
    std::string params = "channel=" + channel + "&ts=" + thread_ts + "&limit=200";
    rate_limiter_.recordCall("conversations.replies");
    auto result = api_->get("conversations.replies", params);
    if (!result || !result->value("ok", false)) return {};

    std::vector<Message> messages;
    if (result->contains("messages")) {
        for (auto& m : (*result)["messages"]) {
            try { messages.push_back(m.get<Message>()); } catch (...) {}
        }
    }
    return messages;
}

bool SlackClient::sendMessage(const ChannelId& channel, const std::string& text,
                               const std::optional<Timestamp>& thread_ts) {
    nlohmann::json body = {{"channel", channel}, {"text", text}};
    if (thread_ts) body["thread_ts"] = *thread_ts;

    rate_limiter_.recordCall("chat.postMessage");
    auto result = api_->post("chat.postMessage", body);
    return result && result->value("ok", false);
}

bool SlackClient::editMessage(const ChannelId& channel, const Timestamp& ts,
                               const std::string& new_text) {
    rate_limiter_.recordCall("chat.update");
    auto result = api_->post("chat.update",
                              {{"channel", channel}, {"ts", ts}, {"text", new_text}});
    return result && result->value("ok", false);
}

bool SlackClient::deleteMessage(const ChannelId& channel, const Timestamp& ts) {
    rate_limiter_.recordCall("chat.delete");
    auto result = api_->post("chat.delete", {{"channel", channel}, {"ts", ts}});
    return result && result->value("ok", false);
}

bool SlackClient::addReaction(const ChannelId& channel, const Timestamp& ts,
                               const std::string& emoji) {
    rate_limiter_.recordCall("reactions.add");
    auto result = api_->post("reactions.add",
                              {{"channel", channel}, {"timestamp", ts}, {"name", emoji}});
    return result && result->value("ok", false);
}

bool SlackClient::removeReaction(const ChannelId& channel, const Timestamp& ts,
                                  const std::string& emoji) {
    rate_limiter_.recordCall("reactions.remove");
    auto result = api_->post("reactions.remove",
                              {{"channel", channel}, {"timestamp", ts}, {"name", emoji}});
    return result && result->value("ok", false);
}

bool SlackClient::uploadFile(const ChannelId& channel, const std::string& filepath,
                              const std::string& title) {
    rate_limiter_.recordCall("files.upload");
    auto result = api_->uploadFile(channel, filepath, title);
    return result && result->value("ok", false);
}

std::vector<uint8_t> SlackClient::downloadFile(const std::string& url) {
    return api_->downloadFile(url);
}

std::optional<User> SlackClient::getUser(const UserId& id) {
    return user_cache_.get(id);
}

std::string SlackClient::displayName(const UserId& id) {
    return user_cache_.displayName(id);
}

std::vector<SlackClient::SearchResult> SlackClient::searchMessages(const std::string& query,
                                                                     int count) {
    std::string params = "query=" + query + "&count=" + std::to_string(count);
    rate_limiter_.recordCall("search.messages");
    auto result = api_->get("search.messages", params);
    if (!result || !result->value("ok", false)) return {};

    std::vector<SearchResult> results;
    if (result->contains("messages") && (*result)["messages"].contains("matches")) {
        for (auto& match : (*result)["messages"]["matches"]) {
            SearchResult sr;
            sr.message = match.get<Message>();
            sr.channel = match.contains("channel") ? match["channel"].value("id", "") : "";
            sr.channel_name = match.contains("channel") ? match["channel"].value("name", "") : "";
            results.push_back(std::move(sr));
        }
    }
    return results;
}

std::vector<Message> SlackClient::getPins(const ChannelId& channel) {
    auto result = api_->get("pins.list", "channel=" + channel);
    if (!result || !result->value("ok", false)) return {};

    std::vector<Message> pins;
    if (result->contains("items")) {
        for (auto& item : (*result)["items"]) {
            if (item.contains("message")) {
                try { pins.push_back(item["message"].get<Message>()); } catch (...) {}
            }
        }
    }
    return pins;
}

bool SlackClient::pinMessage(const ChannelId& channel, const Timestamp& ts) {
    auto result = api_->post("pins.add", {{"channel", channel}, {"timestamp", ts}});
    return result && result->value("ok", false);
}

bool SlackClient::unpinMessage(const ChannelId& channel, const Timestamp& ts) {
    auto result = api_->post("pins.remove", {{"channel", channel}, {"timestamp", ts}});
    return result && result->value("ok", false);
}

void SlackClient::sendTyping(const ChannelId& channel) {
    // typing indicators are sent over socket mode, not REST
    // but slack also accepts them via the web API now
    api_->postForm("users.typing", "channel=" + channel);
}

bool SlackClient::setStatus(const std::string& emoji, const std::string& text,
                             int expiration_minutes) {
    nlohmann::json profile = {
        {"status_emoji", emoji},
        {"status_text", text}
    };
    if (expiration_minutes > 0) {
        auto exp = std::chrono::system_clock::now() +
                   std::chrono::minutes(expiration_minutes);
        profile["status_expiration"] =
            std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count();
    }
    auto result = api_->post("users.profile.set", {{"profile", profile}});
    return result && result->value("ok", false);
}

bool SlackClient::setPresence(bool is_away) {
    auto result = api_->post("users.setPresence",
                              {{"presence", is_away ? "away" : "auto"}});
    return result && result->value("ok", false);
}

bool SlackClient::pollEvent(SlackEvent& event_out) {
    return event_queue_.pop(event_out);
}

std::string SlackClient::connectionState() const {
    if (!api_) return "disconnected";
    if (socket_ && socket_->isConnected()) return "connected";
    if (socket_) return socket_->state();
    return "rest only";
}

} // namespace conduit::slack
