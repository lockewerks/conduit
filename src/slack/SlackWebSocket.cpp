#include "slack/SlackWebSocket.h"
#include "util/Logger.h"

#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

namespace conduit::slack {

SlackWebSocket::SlackWebSocket(const std::string& token, const std::string& cookie)
    : token_(token), cookie_(cookie) {}

SlackWebSocket::~SlackWebSocket() {
    disconnect();
}

int SlackWebSocket::lwsCallback(struct lws* wsi, int reason, void* user,
                                  void* in, size_t len) {
    auto* client = static_cast<SlackWebSocket*>(
        lws_context_user(lws_get_context(wsi)));
    if (!client) return 0;

    switch (static_cast<enum lws_callback_reasons>(reason)) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        LOG_INFO("slack websocket connected");
        client->connected_ = true;
        client->reconnect_delay_ = 1;
        client->last_ping_ = std::chrono::steady_clock::now();
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        if (in && len > 0) {
            client->recv_buffer_.append(static_cast<char*>(in), len);
            if (lws_is_final_fragment(wsi)) {
                client->processMessage(client->recv_buffer_);
                client->recv_buffer_.clear();
            }
        }
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        std::lock_guard<std::mutex> lock(client->send_mutex_);
        if (!client->pending_send_.empty()) {
            std::vector<uint8_t> buf(LWS_PRE + client->pending_send_.size());
            memcpy(buf.data() + LWS_PRE, client->pending_send_.data(),
                   client->pending_send_.size());
            lws_write(wsi, buf.data() + LWS_PRE, client->pending_send_.size(),
                       LWS_WRITE_TEXT);
            client->pending_send_.clear();
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        // inject the d= cookie into the websocket upgrade handshake
        // without this, xoxc tokens get 401 invalid_auth
        if (client && !client->cookie_.empty()) {
            unsigned char **p = (unsigned char **)in;
            unsigned char *end = (*p) + len;
            std::string cookie_val = "d=" + client->cookie_;
            if (lws_add_http_header_by_name(wsi,
                    (const unsigned char *)"Cookie:",
                    (const unsigned char *)cookie_val.c_str(),
                    (int)cookie_val.size(), p, end)) {
                return -1;
            }
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LOG_ERROR(std::string("websocket error: ") +
                  (in ? std::string(static_cast<char*>(in), len) : "unknown"));
        client->connected_ = false;
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        LOG_WARN("websocket closed");
        client->connected_ = false;
        break;

    default:
        break;
    }
    return 0;
}

bool SlackWebSocket::connect() {
    should_stop_ = false;
    ws_thread_ = std::thread(&SlackWebSocket::wsThreadFunc, this);
    return true;
}

void SlackWebSocket::disconnect() {
    should_stop_ = true;
    connected_ = false;

    if (lws_ctx_) {
        lws_cancel_service(lws_ctx_);
    }

    if (ws_thread_.joinable()) {
        ws_thread_.join();
    }

    if (lws_ctx_) {
        lws_context_destroy(lws_ctx_);
        lws_ctx_ = nullptr;
    }
}

void SlackWebSocket::wsThreadFunc() {
    while (!should_stop_) {
        // connect directly to wss-primary.slack.com with token in URL
        // this is exactly what the browser client does
        std::string host = "wss-primary.slack.com";
        // token goes in the URL, cookie goes in the HTTP Cookie header
        // (added via LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER)
        std::string path = "/?token=" + token_ +
                           "&sync_desync=1&slack_client=desktop&batch_presence_aware=1";

        static const struct lws_protocols protocols[] = {
            {"slack-ws", [](struct lws* wsi, enum lws_callback_reasons reason,
                           void* user, void* in, size_t len) -> int {
                 return SlackWebSocket::lwsCallback(wsi, static_cast<int>(reason),
                                                     user, in, len);
             },
             0, 65536},
            {nullptr, nullptr, 0, 0}
        };

        struct lws_context_creation_info ctx_info = {};
        ctx_info.port = CONTEXT_PORT_NO_LISTEN;
        ctx_info.protocols = protocols;
        ctx_info.user = this;
        ctx_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

        lws_ctx_ = lws_create_context(&ctx_info);
        if (!lws_ctx_) {
            LOG_ERROR("failed to create websocket context");
            break;
        }

        struct lws_client_connect_info conn_info = {};
        conn_info.context = lws_ctx_;
        conn_info.address = host.c_str();
        conn_info.port = 443;
        conn_info.path = path.c_str();
        conn_info.host = host.c_str();
        conn_info.origin = "https://app.slack.com";
        conn_info.protocol = "slack-ws";
        conn_info.ssl_connection = LCCSCF_USE_SSL;

        // the d cookie needs to go in the custom headers for xoxc tokens
        std::string custom_headers;
        if (!cookie_.empty()) {
            custom_headers = "Cookie: d=" + cookie_ + "\r\n";
            // lws uses userdata to pass custom headers... actually lws doesn't
            // have a clean way to do this. we'll set it via the protocol.
        }

        wsi_ = lws_client_connect_via_info(&conn_info);
        if (!wsi_) {
            LOG_ERROR("websocket connect failed");
            lws_context_destroy(lws_ctx_);
            lws_ctx_ = nullptr;

            if (should_stop_) break;
            LOG_INFO("reconnecting in " + std::to_string(reconnect_delay_) + "s...");
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_delay_));
            reconnect_delay_ = std::min(reconnect_delay_ * 2, 30);
            continue;
        }

        // service loop with keepalive pings every 30 seconds
        while (!should_stop_ && lws_ctx_) {
            lws_service(lws_ctx_, 100);

            // send a ping every 30 seconds to keep the connection alive
            // slack will drop idle websockets without these
            if (connected_ && wsi_) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_ping_ > std::chrono::seconds(30)) {
                    last_ping_ = now;
                    std::string ping = "{\"type\":\"ping\",\"id\":" +
                                       std::to_string(ping_id_++) + "}";
                    {
                        std::lock_guard<std::mutex> lock(send_mutex_);
                        pending_send_ = ping;
                    }
                    lws_callback_on_writable(wsi_);
                }
            }
        }

        if (lws_ctx_) {
            lws_context_destroy(lws_ctx_);
            lws_ctx_ = nullptr;
        }
        wsi_ = nullptr;

        if (!should_stop_) {
            LOG_INFO("reconnecting in " + std::to_string(reconnect_delay_) + "s...");
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_delay_));
            reconnect_delay_ = std::min(reconnect_delay_ * 2, 30);
        }
    }
}

void SlackWebSocket::processMessage(const std::string& msg) {
    try {
        auto j = nlohmann::json::parse(msg);

        std::string type = j.value("type", "");

        if (type == "hello") {
            LOG_INFO("slack websocket handshake complete");
            return;
        }

        if (type == "pong") {
            return;
        }

        if (msg_callback_) {
            msg_callback_(j);
        }
    } catch (...) {
        // not all messages are JSON (binary pings, etc)
    }
}

std::string SlackWebSocket::state() const {
    if (connected_) return "connected";
    if (should_stop_) return "disconnected";
    return "reconnecting";
}

} // namespace conduit::slack
