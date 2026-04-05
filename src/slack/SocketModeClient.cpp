#include "slack/SocketModeClient.h"
#include "util/Logger.h"

#include <curl/curl.h>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

namespace conduit::slack {

// write callback for the initial REST call to get websocket URL
static size_t curlWrite(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

SocketModeClient::SocketModeClient(const std::string& app_token)
    : app_token_(app_token) {}

SocketModeClient::~SocketModeClient() {
    disconnect();
}

bool SocketModeClient::fetchWSUrl() {
    // POST to apps.connections.open with the app-level token
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string url = "https://slack.com/api/apps.connections.open";

    struct curl_slist* headers = nullptr;
    std::string auth = "Authorization: Bearer " + app_token_;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR(std::string("failed to get socket mode URL: ") + curl_easy_strerror(res));
        return false;
    }

    try {
        auto j = nlohmann::json::parse(response);
        if (j.value("ok", false)) {
            ws_url_ = j.value("url", "");
            LOG_INFO("got socket mode URL");
            return !ws_url_.empty();
        }
        LOG_ERROR("apps.connections.open failed: " + j.value("error", "unknown"));
    } catch (...) {
        LOG_ERROR("failed to parse socket mode response");
    }
    return false;
}

int SocketModeClient::lwsCallback(struct lws* wsi, int reason, void* user,
                                   void* in, size_t len) {
    auto* client = static_cast<SocketModeClient*>(
        lws_context_user(lws_get_context(wsi)));
    if (!client) return 0;

    switch (static_cast<enum lws_callback_reasons>(reason)) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        LOG_INFO("socket mode websocket connected");
        client->connected_ = true;
        client->reconnect_delay_ = 1;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        if (in && len > 0) {
            client->recv_buffer_.append(static_cast<char*>(in), len);
            // check if this is the final fragment
            if (lws_is_final_fragment(wsi)) {
                client->processMessage(client->recv_buffer_);
                client->recv_buffer_.clear();
            }
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        std::lock_guard<std::mutex> lock(client->send_mutex_);
        if (!client->pending_send_.empty()) {
            // lws needs LWS_PRE bytes of padding before the data
            std::vector<uint8_t> buf(LWS_PRE + client->pending_send_.size());
            memcpy(buf.data() + LWS_PRE, client->pending_send_.data(),
                   client->pending_send_.size());
            lws_write(wsi, buf.data() + LWS_PRE, client->pending_send_.size(),
                       LWS_WRITE_TEXT);
            client->pending_send_.clear();
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LOG_ERROR(std::string("websocket connection error: ") +
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

bool SocketModeClient::connect() {
    if (!fetchWSUrl()) return false;

    should_stop_ = false;
    ws_thread_ = std::thread(&SocketModeClient::wsThreadFunc, this);
    return true;
}

void SocketModeClient::disconnect() {
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

void SocketModeClient::wsThreadFunc() {
    while (!should_stop_) {
        // parse the wss:// URL
        // format: wss://wss-something.slack.com/link/?ticket=xxx&app_id=xxx
        std::string url = ws_url_;
        if (url.substr(0, 6) == "wss://") url = url.substr(6);

        auto path_pos = url.find('/');
        std::string host = url.substr(0, path_pos);
        std::string path = path_pos != std::string::npos ? url.substr(path_pos) : "/";

        struct lws_context_creation_info ctx_info = {};
        ctx_info.port = CONTEXT_PORT_NO_LISTEN;
        ctx_info.protocols = nullptr; // we use the default
        ctx_info.user = this;
        ctx_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

        // set up our protocol
        static const struct lws_protocols protocols[] = {
            {"conduit-ws", [](struct lws* wsi, enum lws_callback_reasons reason,
                             void* user, void* in, size_t len) -> int {
                 return SocketModeClient::lwsCallback(wsi, static_cast<int>(reason),
                                                       user, in, len);
             },
             0, 65536},
            {nullptr, nullptr, 0, 0}};
        ctx_info.protocols = protocols;

        lws_ctx_ = lws_create_context(&ctx_info);
        if (!lws_ctx_) {
            LOG_ERROR("failed to create lws context");
            break;
        }

        struct lws_client_connect_info conn_info = {};
        conn_info.context = lws_ctx_;
        conn_info.address = host.c_str();
        conn_info.port = 443;
        conn_info.path = path.c_str();
        conn_info.host = host.c_str();
        conn_info.origin = host.c_str();
        conn_info.protocol = "conduit-ws";
        conn_info.ssl_connection = LCCSCF_USE_SSL;

        wsi_ = lws_client_connect_via_info(&conn_info);
        if (!wsi_) {
            LOG_ERROR("websocket connect failed");
            lws_context_destroy(lws_ctx_);
            lws_ctx_ = nullptr;

            // exponential backoff
            if (should_stop_) break;
            LOG_INFO("reconnecting in " + std::to_string(reconnect_delay_) + "s...");
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_delay_));
            reconnect_delay_ = std::min(reconnect_delay_ * 2, 30);
            continue;
        }

        // service loop
        while (!should_stop_ && lws_ctx_) {
            lws_service(lws_ctx_, 100); // 100ms timeout
        }

        if (lws_ctx_) {
            lws_context_destroy(lws_ctx_);
            lws_ctx_ = nullptr;
        }
        wsi_ = nullptr;

        // if we got disconnected and shouldn't stop, reconnect
        if (!should_stop_) {
            LOG_INFO("reconnecting in " + std::to_string(reconnect_delay_) + "s...");
            std::this_thread::sleep_for(std::chrono::seconds(reconnect_delay_));
            reconnect_delay_ = std::min(reconnect_delay_ * 2, 30);

            // get a fresh URL for reconnection
            if (!fetchWSUrl()) {
                LOG_ERROR("couldn't get new websocket URL for reconnect");
                continue;
            }
        }
    }
}

void SocketModeClient::processMessage(const std::string& msg) {
    try {
        auto j = nlohmann::json::parse(msg);

        // hello message on connect
        if (j.value("type", "") == "hello") {
            LOG_INFO("socket mode handshake complete");
            return;
        }

        // disconnect request from slack (they do this for maintenance)
        if (j.value("type", "") == "disconnect") {
            LOG_WARN("slack requested disconnect: " + j.value("reason", "unknown"));
            connected_ = false;
            return;
        }

        // acknowledge the envelope immediately (slack requires this)
        std::string envelope_id = j.value("envelope_id", "");
        if (!envelope_id.empty()) {
            acknowledge(envelope_id);
        }

        // pass the payload to our callback
        if (event_callback_ && j.contains("payload")) {
            event_callback_(j["payload"]);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("failed to process ws message: ") + e.what());
    }
}

void SocketModeClient::acknowledge(const std::string& envelope_id) {
    nlohmann::json ack = {{"envelope_id", envelope_id}};
    std::string ack_str = ack.dump();

    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        pending_send_ = ack_str;
    }

    if (wsi_) {
        lws_callback_on_writable(wsi_);
    }
}

std::string SocketModeClient::state() const {
    if (connected_) return "connected";
    if (should_stop_) return "disconnected";
    return "reconnecting";
}

} // namespace conduit::slack
