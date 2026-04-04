#pragma once
#include "slack/Types.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

// forward declare lws stuff so we don't leak it into every header
struct lws_context;
struct lws;

namespace conduit::slack {

// websocket client for slack's socket mode
// runs in its own thread, pushes events into a ThreadSafeQueue
class SocketModeClient {
public:
    using EventCallback = std::function<void(const nlohmann::json& payload)>;

    explicit SocketModeClient(const std::string& app_token);
    ~SocketModeClient();

    // connect to slack's socket mode endpoint
    // this spawns a background thread that stays alive
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_.load(); }

    // set the callback for incoming events
    void setEventCallback(EventCallback cb) { event_callback_ = std::move(cb); }

    // connection state
    std::string state() const;

private:
    std::string app_token_;
    std::string ws_url_;
    EventCallback event_callback_;

    std::thread ws_thread_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> should_stop_{false};
    int reconnect_delay_ = 1;

    lws_context* lws_ctx_ = nullptr;
    lws* wsi_ = nullptr;
    std::mutex send_mutex_;
    std::string pending_send_;

    // get the websocket URL from slack
    bool fetchWSUrl();

    // the websocket thread main loop
    void wsThreadFunc();

    // acknowledge a socket mode envelope (required or slack disconnects you)
    void acknowledge(const std::string& envelope_id);

    // libwebsockets callback (static because lws is C)
    static int lwsCallback(struct lws* wsi, int reason, void* user, void* in, size_t len);

    // buffer for assembling fragmented ws messages
    std::string recv_buffer_;

    // process a complete received message
    void processMessage(const std::string& msg);
};

} // namespace conduit::slack
