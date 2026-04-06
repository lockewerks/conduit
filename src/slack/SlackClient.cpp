#include "slack/SlackClient.h"
#include "util/Logger.h"

namespace conduit::slack {

SlackClient::SlackClient(const OrgConfig& config, cache::Database& db)
    : config_(config), db_(db), channel_cache_(db), user_cache_(db),
      bookmark_cache_(db), reminder_cache_(db), draft_store_(db),
      user_group_cache_(db), saved_item_cache_(db) {}

SlackClient::~SlackClient() {
    disconnect();
}

bool SlackClient::connect() {
    LOG_INFO("connecting to " + config_.name + "...");

    // set up the REST API client
    api_ = std::make_unique<WebAPI>(config_.user_token);
    if (!config_.d_cookie.empty()) {
        api_->setCookie(config_.d_cookie);
    }

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

    // real-time events: try socket mode first (app token), fall back to
    // direct websocket (same as the browser client uses)
    if (!config_.app_token.empty()) {
        // socket mode: app token + bot event subscriptions
        socket_ = std::make_unique<SocketModeClient>(config_.app_token);
        socket_->setEventCallback([this](const nlohmann::json& payload) {
            auto event = dispatcher_->dispatch(payload);
            if (event) {
                event_queue_.push(std::move(*event));
            }
        });
        if (!socket_->connect()) {
            LOG_WARN("socket mode failed, falling back to direct websocket");
        }
    }

    // if no socket mode (or it failed), use direct websocket like the browser
    if (!socket_ || !socket_->isConnected()) {
        web_socket_ = std::make_unique<SlackWebSocket>(config_.user_token, config_.d_cookie);
        web_socket_->setMessageCallback([this](const nlohmann::json& msg) {
            // the web client websocket sends events directly, not wrapped
            // in socket mode envelopes. the "type" field is at the top level.
            std::string type = msg.value("type", "");

            if (type == "message") {
                SlackEvent e;
                std::string subtype = msg.value("subtype", "");
                if (subtype == "message_changed") {
                    e.type = SlackEvent::Type::MessageChanged;
                    if (msg.contains("message")) {
                        e.message = msg["message"].get<Message>();
                    }
                } else if (subtype == "message_deleted") {
                    e.type = SlackEvent::Type::MessageDeleted;
                    e.ts = msg.value("deleted_ts", "");
                } else {
                    e.type = SlackEvent::Type::MessageNew;
                    try { e.message = msg.get<Message>(); } catch (...) {}
                }
                e.channel = msg.value("channel", "");
                e.user = msg.value("user", "");
                e.ts = msg.value("ts", "");
                event_queue_.push(std::move(e));
            } else if (type == "reaction_added" || type == "reaction_removed") {
                SlackEvent e;
                e.type = (type == "reaction_added") ? SlackEvent::Type::ReactionAdded
                                                     : SlackEvent::Type::ReactionRemoved;
                e.user = msg.value("user", "");
                if (msg.contains("item")) {
                    e.channel = msg["item"].value("channel", "");
                    e.ts = msg["item"].value("ts", "");
                }
                Reaction r;
                r.emoji_name = msg.value("reaction", "");
                r.count = 1;
                r.users = {e.user};
                r.user_reacted = (e.user == auth_info_.user_id);
                e.reaction = r;
                event_queue_.push(std::move(e));
            } else if (type == "channel_marked") {
                SlackEvent e;
                e.type = SlackEvent::Type::ChannelMarked;
                e.channel = msg.value("channel", "");
                event_queue_.push(std::move(e));
            } else if (type == "user_typing") {
                SlackEvent e;
                e.type = SlackEvent::Type::TypingStart;
                e.channel = msg.value("channel", "");
                e.user = msg.value("user", "");
                event_queue_.push(std::move(e));
            } else if (type == "presence_change") {
                SlackEvent e;
                e.type = SlackEvent::Type::PresenceChange;
                e.user = msg.value("user", "");
                e.is_online = (msg.value("presence", "") == "active");
                event_queue_.push(std::move(e));
            } else if (type == "bookmark_added" || type == "bookmark_deleted") {
                SlackEvent e;
                e.type = (type == "bookmark_added") ? SlackEvent::Type::BookmarkAdded
                                                     : SlackEvent::Type::BookmarkRemoved;
                if (msg.contains("bookmark")) {
                    e.bookmark = msg["bookmark"].get<Bookmark>();
                }
                e.channel = msg.value("channel_id", "");
                event_queue_.push(std::move(e));
            } else if (type == "dnd_updated" || type == "dnd_updated_user") {
                SlackEvent e;
                e.type = SlackEvent::Type::DndUpdated;
                if (msg.contains("dnd_status")) {
                    e.dnd_status = msg["dnd_status"].get<DndStatus>();
                }
                event_queue_.push(std::move(e));
            } else if (type == "reminder_fired") {
                SlackEvent e;
                e.type = SlackEvent::Type::ReminderFired;
                if (msg.contains("reminder")) {
                    e.reminder = msg["reminder"].get<Reminder>();
                }
                event_queue_.push(std::move(e));
            } else if (type == "star_added" || type == "star_removed") {
                SlackEvent e;
                e.type = (type == "star_added") ? SlackEvent::Type::StarAdded
                                                 : SlackEvent::Type::StarRemoved;
                if (msg.contains("item")) {
                    e.channel = msg["item"].value("channel", "");
                    e.ts = msg["item"].value("message", nlohmann::json{}).value("ts", "");
                }
                event_queue_.push(std::move(e));
            } else if (type == "subteam_updated" || type == "subteam_members_changed") {
                SlackEvent e;
                e.type = SlackEvent::Type::UserGroupUpdated;
                if (msg.contains("subteam")) {
                    e.user_group = msg["subteam"].get<UserGroup>();
                }
                event_queue_.push(std::move(e));
            } else if (type == "channel_unarchive") {
                SlackEvent e;
                e.type = SlackEvent::Type::ChannelUnarchived;
                e.channel = msg.value("channel", "");
                event_queue_.push(std::move(e));
            }
            // ignore types we don't care about (pref_change, etc)
        });
        web_socket_->connect();
        LOG_INFO("direct websocket started (browser-style connection)");
    }

    // post a connection event
    SlackEvent conn_event;
    conn_event.type = SlackEvent::Type::ConnectionStatus;
    conn_event.connection_state = "connected";
    event_queue_.push(conn_event);

    return true;
}

void SlackClient::disconnect() {
    if (web_socket_) {
        web_socket_->disconnect();
        web_socket_.reset();
    }
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

    // fetch user groups
    auto groups = getUserGroups(true);
    user_group_cache_.loadFromAPI(groups);

    // fetch DND status
    dnd_status_ = getDndStatus();

    // load cached data for new features from DB
    bookmark_cache_.loadFromDB();
    reminder_cache_.loadFromDB();
    draft_store_.loadFromDB();
    saved_item_cache_.loadFromDB();

    // fetch fresh reminders + saved items (small payloads)
    auto reminders = getReminders();
    reminder_cache_.loadFromAPI(reminders);

    LOG_INFO("bootstrap complete: " + std::to_string(channels.size()) + " channels, " +
             std::to_string(users.size()) + " users, " +
             std::to_string(groups.size()) + " user groups");
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

std::optional<ChannelId> SlackClient::openDM(const UserId& user_id) {
    rate_limiter_.recordCall("conversations.open");
    LOG_INFO("opening DM with user: " + user_id);

    // check if we already have a DM channel open for this user
    for (auto& ch : channel_cache_.getAll()) {
        if (ch.type == ChannelType::DirectMessage && ch.dm_user_id == user_id && ch.is_member) {
            LOG_INFO("found existing DM channel: " + ch.id);
            return ch.id;
        }
    }

    // try form-encoded first (more reliable with xoxc tokens), then JSON
    auto result = api_->postForm("conversations.open",
                                  "users=" + user_id + "&return_im=true");
    if (!result || !result->value("ok", false)) {
        std::string err = result ? result->value("error", "unknown") : "no response";
        LOG_WARN("conversations.open form failed (" + err + "), trying JSON");
        result = api_->post("conversations.open", {{"users", user_id}, {"return_im", true}});
    }

    if (!result) {
        LOG_ERROR("conversations.open returned no result for user: " + user_id);
        return std::nullopt;
    }
    if (!result->value("ok", false)) {
        LOG_ERROR("conversations.open failed: " + result->value("error", "unknown"));
        return std::nullopt;
    }

    if (result->contains("channel") && (*result)["channel"].contains("id")) {
        ChannelId ch_id = (*result)["channel"]["id"].get<std::string>();
        LOG_INFO("opened DM channel: " + ch_id);

        // make sure it's in our channel cache with is_member=true
        Channel ch;
        ch.id = ch_id;
        ch.type = ChannelType::DirectMessage;
        ch.dm_user_id = user_id;
        ch.name = displayName(user_id);
        ch.is_member = true;
        channel_cache_.upsert(ch);

        return ch_id;
    }

    LOG_ERROR("conversations.open response missing channel.id");
    return std::nullopt;
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
                // slack's attachment format is a nightmare. giphy gifs, unfurled
                // images, and shared links all come through as "attachments" not "files".
                // and the image_url can be at the top level OR nested inside blocks.
                // we scan everything and promote any image URL we find into the files
                // array so our renderer can actually display them.
                if (m.contains("attachments") && m["attachments"].is_array()) {
                    for (auto& a : m["attachments"]) {
                        std::string img_url;
                        int img_w = 0, img_h = 0;
                        std::string title = a.value("fallback", "image");
                        bool is_animated = false;

                        // check top-level image_url first
                        img_url = a.value("image_url", "");
                        img_w = a.value("image_width", 0);
                        img_h = a.value("image_height", 0);

                        // if not there, dig into blocks (slack's newer format)
                        if (img_url.empty() && a.contains("blocks") && a["blocks"].is_array()) {
                            for (auto& block : a["blocks"]) {
                                if (block.value("type", "") == "image") {
                                    img_url = block.value("image_url", "");
                                    img_w = block.value("image_width", 0);
                                    img_h = block.value("image_height", 0);
                                    is_animated = block.value("is_animated", false);
                                    if (block.contains("title") && block["title"].is_object()) {
                                        title = block["title"].value("text", title);
                                    }
                                    if (!img_url.empty()) break;
                                }
                            }
                        }

                        if (!img_url.empty()) {
                            SlackFile sf;
                            sf.name = title;
                            sf.url_private = img_url;
                            sf.thumb_360 = img_url; // no separate thumb, use full URL
                            sf.original_w = img_w;
                            sf.original_h = img_h;
                            // figure out if it's a gif or static image
                            if (is_animated || img_url.find(".gif") != std::string::npos) {
                                sf.mimetype = "image/gif";
                            } else if (img_url.find(".png") != std::string::npos) {
                                sf.mimetype = "image/png";
                            } else {
                                sf.mimetype = "image/jpeg";
                            }
                            msg.files.push_back(sf);
                            LOG_INFO("found image in attachment: " + img_url.substr(0, 80));
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
                               const std::optional<Timestamp>& thread_ts,
                               bool reply_broadcast) {
    nlohmann::json body = {{"channel", channel}, {"text", text}};
    if (thread_ts) {
        body["thread_ts"] = *thread_ts;
        if (reply_broadcast) {
            body["reply_broadcast"] = true;
        }
    }

    rate_limiter_.recordCall("chat.postMessage");
    auto result = api_->post("chat.postMessage", body);
    if (!result) {
        LOG_ERROR("sendMessage: no response");
        return false;
    }
    if (!result->value("ok", false)) {
        LOG_ERROR("sendMessage failed: " + result->value("error", "unknown"));
        return false;
    }
    if (thread_ts && reply_broadcast) {
        LOG_INFO("sent broadcast reply to thread " + *thread_ts);
    }
    return true;
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

std::unordered_map<std::string, std::string> SlackClient::getCustomEmoji() {
    rate_limiter_.recordCall("emoji.list");
    auto result = api_->get("emoji.list");
    std::unordered_map<std::string, std::string> emoji;
    if (!result || !result->value("ok", false)) return emoji;

    if (result->contains("emoji") && (*result)["emoji"].is_object()) {
        for (auto& [name, url] : (*result)["emoji"].items()) {
            std::string url_str = url.get<std::string>();
            // skip aliases (they start with "alias:")
            if (url_str.find("alias:") == 0) continue;
            emoji[name] = url_str;
        }
    }
    LOG_INFO("fetched " + std::to_string(emoji.size()) + " custom emoji");
    return emoji;
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

// -- channel creation --

std::optional<Channel> SlackClient::createChannel(const std::string& name, bool is_private) {
    nlohmann::json body = {{"name", name}, {"is_private", is_private}};
    rate_limiter_.recordCall("conversations.create");
    auto result = api_->post("conversations.create", body);
    if (!result || !result->value("ok", false)) {
        LOG_ERROR("createChannel failed: " + (result ? result->value("error", "unknown") : "no response"));
        return std::nullopt;
    }
    if (result->contains("channel")) {
        auto ch = (*result)["channel"].get<Channel>();
        ch.is_member = true;
        channel_cache_.upsert(ch);
        return ch;
    }
    return std::nullopt;
}

bool SlackClient::archiveChannel(const ChannelId& id) {
    rate_limiter_.recordCall("conversations.archive");
    auto result = api_->post("conversations.archive", {{"channel", id}});
    return result && result->value("ok", false);
}

// -- bookmarks --

std::vector<Bookmark> SlackClient::getBookmarks(const ChannelId& channel) {
    rate_limiter_.recordCall("bookmarks.list");
    auto result = api_->get("bookmarks.list", "channel_id=" + channel);
    if (!result || !result->value("ok", false)) return {};

    std::vector<Bookmark> bookmarks;
    if (result->contains("bookmarks") && (*result)["bookmarks"].is_array()) {
        for (auto& b : (*result)["bookmarks"]) {
            try { bookmarks.push_back(b.get<Bookmark>()); } catch (...) {}
        }
    }
    bookmark_cache_.loadForChannel(channel, bookmarks);
    return bookmarks;
}

bool SlackClient::addBookmark(const ChannelId& channel, const std::string& title,
                               const std::string& link) {
    rate_limiter_.recordCall("bookmarks.add");
    auto result = api_->post("bookmarks.add",
                              {{"channel_id", channel}, {"title", title}, {"link", link}, {"type", "link"}});
    return result && result->value("ok", false);
}

bool SlackClient::removeBookmark(const ChannelId& channel, const std::string& bookmark_id) {
    rate_limiter_.recordCall("bookmarks.remove");
    auto result = api_->post("bookmarks.remove",
                              {{"channel_id", channel}, {"bookmark_id", bookmark_id}});
    return result && result->value("ok", false);
}

// -- scheduled messages --

bool SlackClient::scheduleMessage(const ChannelId& channel, const std::string& text,
                                   int64_t post_at, const std::optional<Timestamp>& thread_ts) {
    nlohmann::json body = {{"channel", channel}, {"text", text}, {"post_at", post_at}};
    if (thread_ts) body["thread_ts"] = *thread_ts;
    rate_limiter_.recordCall("chat.scheduleMessage");
    auto result = api_->post("chat.scheduleMessage", body);
    if (!result || !result->value("ok", false)) {
        LOG_ERROR("scheduleMessage failed: " + (result ? result->value("error", "unknown") : "no response"));
        return false;
    }
    return true;
}

std::vector<ScheduledMessage> SlackClient::getScheduledMessages(const ChannelId& channel) {
    nlohmann::json body = {};
    if (!channel.empty()) body["channel"] = channel;
    rate_limiter_.recordCall("chat.scheduledMessages.list");
    auto result = api_->post("chat.scheduledMessages.list", body);
    if (!result || !result->value("ok", false)) return {};

    std::vector<ScheduledMessage> msgs;
    if (result->contains("scheduled_messages") && (*result)["scheduled_messages"].is_array()) {
        for (auto& m : (*result)["scheduled_messages"]) {
            try { msgs.push_back(m.get<ScheduledMessage>()); } catch (...) {}
        }
    }
    return msgs;
}

bool SlackClient::deleteScheduledMessage(const ChannelId& channel,
                                          const std::string& scheduled_id) {
    rate_limiter_.recordCall("chat.deleteScheduledMessage");
    auto result = api_->post("chat.deleteScheduledMessage",
                              {{"channel", channel}, {"scheduled_message_id", scheduled_id}});
    return result && result->value("ok", false);
}

// -- reminders --

std::vector<Reminder> SlackClient::getReminders() {
    rate_limiter_.recordCall("reminders.list");
    auto result = api_->get("reminders.list");
    if (!result || !result->value("ok", false)) return {};

    std::vector<Reminder> reminders;
    if (result->contains("reminders") && (*result)["reminders"].is_array()) {
        for (auto& r : (*result)["reminders"]) {
            try { reminders.push_back(r.get<Reminder>()); } catch (...) {}
        }
    }
    return reminders;
}

bool SlackClient::addReminder(const std::string& text, int64_t time,
                               const std::string& user_or_channel) {
    nlohmann::json body = {{"text", text}, {"time", time}};
    if (!user_or_channel.empty()) body["user"] = user_or_channel;
    rate_limiter_.recordCall("reminders.add");
    auto result = api_->post("reminders.add", body);
    return result && result->value("ok", false);
}

bool SlackClient::deleteReminder(const std::string& reminder_id) {
    rate_limiter_.recordCall("reminders.delete");
    auto result = api_->post("reminders.delete", {{"reminder", reminder_id}});
    return result && result->value("ok", false);
}

bool SlackClient::completeReminder(const std::string& reminder_id) {
    rate_limiter_.recordCall("reminders.complete");
    auto result = api_->post("reminders.complete", {{"reminder", reminder_id}});
    return result && result->value("ok", false);
}

// -- user groups --

std::vector<UserGroup> SlackClient::getUserGroups(bool include_users) {
    std::string params = "include_users=" + std::string(include_users ? "true" : "false");
    rate_limiter_.recordCall("usergroups.list");
    auto result = api_->get("usergroups.list", params);
    if (!result || !result->value("ok", false)) return {};

    std::vector<UserGroup> groups;
    if (result->contains("usergroups") && (*result)["usergroups"].is_array()) {
        for (auto& g : (*result)["usergroups"]) {
            try { groups.push_back(g.get<UserGroup>()); } catch (...) {}
        }
    }
    return groups;
}

// -- DND --

DndStatus SlackClient::getDndStatus() {
    rate_limiter_.recordCall("dnd.info");
    auto result = api_->get("dnd.info");
    if (!result || !result->value("ok", false)) return {};
    return result->get<DndStatus>();
}

bool SlackClient::setSnooze(int minutes) {
    rate_limiter_.recordCall("dnd.setSnooze");
    auto result = api_->post("dnd.setSnooze", {{"num_minutes", minutes}});
    if (result && result->value("ok", false)) {
        dnd_status_.snooze_enabled = true;
        return true;
    }
    return false;
}

bool SlackClient::endSnooze() {
    rate_limiter_.recordCall("dnd.endSnooze");
    auto result = api_->post("dnd.endSnooze", {});
    if (result && result->value("ok", false)) {
        dnd_status_.snooze_enabled = false;
        return true;
    }
    return false;
}

// -- saved items --

std::vector<SavedItem> SlackClient::getSavedItems() {
    rate_limiter_.recordCall("stars.list");
    auto result = api_->get("stars.list", "count=200");
    if (!result || !result->value("ok", false)) return {};

    std::vector<SavedItem> items;
    if (result->contains("items") && (*result)["items"].is_array()) {
        for (auto& item : (*result)["items"]) {
            if (item.value("type", "") == "message") {
                SavedItem si;
                si.type = "message";
                si.channel_id = item.value("channel", "");
                if (item.contains("message")) {
                    si.message_ts = item["message"].value("ts", "");
                }
                si.date_saved = item.value("date_create", int64_t(0));
                items.push_back(si);
            }
        }
    }
    return items;
}

bool SlackClient::saveItem(const ChannelId& channel, const Timestamp& ts) {
    rate_limiter_.recordCall("stars.add");
    auto result = api_->post("stars.add", {{"channel", channel}, {"timestamp", ts}});
    if (result && result->value("ok", false)) {
        SavedItem si;
        si.type = "message";
        si.channel_id = channel;
        si.message_ts = ts;
        saved_item_cache_.add(si);
        return true;
    }
    return false;
}

bool SlackClient::removeSavedItem(const ChannelId& channel, const Timestamp& ts) {
    rate_limiter_.recordCall("stars.remove");
    auto result = api_->post("stars.remove", {{"channel", channel}, {"timestamp", ts}});
    if (result && result->value("ok", false)) {
        saved_item_cache_.remove(channel, ts);
        return true;
    }
    return false;
}

// -- files listing --

SlackClient::FileListResult SlackClient::listFiles(const ChannelId& channel, int count, int page) {
    std::string params = "count=" + std::to_string(count) + "&page=" + std::to_string(page);
    if (!channel.empty()) params += "&channel=" + channel;
    rate_limiter_.recordCall("files.list");
    auto result = api_->get("files.list", params);
    if (!result || !result->value("ok", false)) return {};

    FileListResult flr;
    if (result->contains("paging")) {
        flr.total = (*result)["paging"].value("total", 0);
    }
    if (result->contains("files") && (*result)["files"].is_array()) {
        for (auto& f : (*result)["files"]) {
            try { flr.files.push_back(f.get<SlackFile>()); } catch (...) {}
        }
    }
    return flr;
}

// -- user profile (full) --

std::optional<User> SlackClient::getUserProfile(const UserId& id) {
    rate_limiter_.recordCall("users.info");
    auto result = api_->get("users.info", "user=" + id);
    if (!result || !result->value("ok", false)) return std::nullopt;

    if (result->contains("user")) {
        auto user = (*result)["user"].get<User>();
        user_cache_.upsert(user);
        return user;
    }
    return std::nullopt;
}

bool SlackClient::pollEvent(SlackEvent& event_out) {
    return event_queue_.pop(event_out);
}

std::string SlackClient::connectionState() const {
    if (!api_) return "disconnected";
    if (socket_ && socket_->isConnected()) return "connected";
    if (web_socket_ && web_socket_->isConnected()) return "connected";
    if (web_socket_) return web_socket_->state();
    if (socket_) return socket_->state();
    return "connected"; // API works even without websocket
}

} // namespace conduit::slack
