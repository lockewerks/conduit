#pragma once
#include "slack/Types.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

struct lws_context;
struct lws;

namespace conduit::slack {

// direct websocket connection to slack, same as the browser client.
// connects to wss-primary.slack.com with the xoxc token in the URL.
// no app tokens, no socket mode, no bot event subscriptions needed.
// just raw websocket like it's 2015 and RTM was still cool.
class SlackWebSocket {
public:
    using MessageCallback = std::function<void(const nlohmann::json& msg)>;

    SlackWebSocket(const std::string& token, const std::string& cookie = "");
    ~SlackWebSocket();

    bool connect();
    void disconnect();
    bool isConnected() const { return connected_.load(); }

    void setMessageCallback(MessageCallback cb) { msg_callback_ = std::move(cb); }

    std::string state() const;

private:
    std::string token_;
    std::string cookie_;
    MessageCallback msg_callback_;

    std::thread ws_thread_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> should_stop_{false};
    int reconnect_delay_ = 1;

    lws_context* lws_ctx_ = nullptr;
    lws* wsi_ = nullptr;
    std::string recv_buffer_;

    void wsThreadFunc();
    void processMessage(const std::string& msg);

    static int lwsCallback(struct lws* wsi, int reason, void* user, void* in, size_t len);
};

} // namespace conduit::slack
