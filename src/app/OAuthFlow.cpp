#include "app/OAuthFlow.h"
#include "util/Logger.h"
#include "util/Platform.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

namespace conduit {

// the scopes we need. this is everything the official client uses
// minus the admin stuff we don't care about.
static const char* OAUTH_SCOPES =
    "channels:read,channels:write,channels:history,"
    "groups:read,groups:write,groups:history,"
    "im:read,im:write,im:history,"
    "mpim:read,mpim:write,mpim:history,"
    "users:read,users:read.email,users:write,"
    "reactions:read,reactions:write,"
    "pins:read,pins:write,"
    "files:read,files:write,"
    "search:read,emoji:read,chat:write,usergroups:read";

static size_t curlWriteCb(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

OAuthFlow::OAuthFlow(const std::string& client_id, const std::string& client_secret)
    : client_id_(client_id), client_secret_(client_secret) {}

int OAuthFlow::findAvailablePort() {
    // fixed port so the redirect URI matches what's registered in the Slack app.
    // if 17847 is busy, tough luck (but it won't be, it's obscure enough).
    return 17847;
}

OAuthResult OAuthFlow::execute() {
    port_ = findAvailablePort();
    std::string redirect_uri = "http://localhost:" + std::to_string(port_) + "/callback";

    // build the authorize URL
    std::string auth_url = "https://slack.com/oauth/v2/authorize?"
        "client_id=" + client_id_ +
        "&user_scope=" + OAUTH_SCOPES +
        "&redirect_uri=" + redirect_uri;

    LOG_INFO("opening browser for Slack OAuth (port " + std::to_string(port_) + ")");

    // open the browser
    platform::openURL(auth_url);

    // start a tiny HTTP server to catch the callback
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        result_.error = "couldn't create socket";
        result_.ok = false;
        return result_;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in srv_addr = {};
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    srv_addr.sin_port = htons(port_);

    if (bind(server, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) != 0) {
        closesocket(server);
        result_.error = "couldn't bind to port " + std::to_string(port_);
        result_.ok = false;
        return result_;
    }

    listen(server, 1);
    LOG_INFO("waiting for OAuth callback on localhost:" + std::to_string(port_));

    // set a timeout so we don't block forever
    struct timeval tv;
    tv.tv_sec = 120; // 2 minutes should be plenty
    tv.tv_usec = 0;
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    SOCKET client = accept(server, NULL, NULL);
    if (client == INVALID_SOCKET) {
        closesocket(server);
        result_.error = "OAuth timeout - no callback received";
        result_.ok = false;
        return result_;
    }

    // read the HTTP request
    char buf[4096] = {};
    recv(client, buf, sizeof(buf) - 1, 0);
    std::string request(buf);

    // extract the code from "GET /callback?code=XXXXX HTTP/1.1"
    std::string code;
    auto code_pos = request.find("code=");
    if (code_pos != std::string::npos) {
        auto code_end = request.find_first_of("& ", code_pos + 5);
        code = request.substr(code_pos + 5,
                               code_end != std::string::npos ? code_end - (code_pos + 5) : std::string::npos);
    }

    // send a response so the browser shows something nice
    std::string html;
    if (!code.empty()) {
        html = "<!DOCTYPE html><html><body style='background:#000;color:#0f0;font-family:monospace;padding:40px;'>"
               "<h1>Conduit</h1><p>Authentication successful. You can close this tab.</p>"
               "<p style='color:#666;'>token exchange in progress...</p></body></html>";
    } else {
        html = "<!DOCTYPE html><html><body style='background:#000;color:#f44;font-family:monospace;padding:40px;'>"
               "<h1>Conduit</h1><p>Authentication failed. No authorization code received.</p>"
               "<p>Check the Conduit window for details.</p></body></html>";
    }

    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                           "Content-Length: " + std::to_string(html.size()) + "\r\n"
                           "Connection: close\r\n\r\n" + html;
    send(client, response.c_str(), (int)response.size(), 0);

    closesocket(client);
    closesocket(server);

#ifdef _WIN32
    WSACleanup();
#endif

    if (code.empty()) {
        result_.error = "no authorization code in callback";
        result_.ok = false;
        return result_;
    }

    LOG_INFO("got OAuth code, exchanging for token...");
    result_ = exchangeCode(code, redirect_uri);
    return result_;
}

OAuthResult OAuthFlow::exchangeCode(const std::string& code, const std::string& redirect_uri) {
    OAuthResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl init failed";
        return result;
    }

    // POST to oauth.v2.access
    std::string post_data = "client_id=" + client_id_ +
                            "&client_secret=" + client_secret_ +
                            "&code=" + code +
                            "&redirect_uri=" + redirect_uri;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://slack.com/api/oauth.v2.access");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error = "token exchange request failed";
        return result;
    }

    try {
        auto j = nlohmann::json::parse(response);
        result.ok = j.value("ok", false);

        if (result.ok) {
            // the user token is in authed_user.access_token
            if (j.contains("authed_user")) {
                result.access_token = j["authed_user"].value("access_token", "");
                result.user_id = j["authed_user"].value("id", "");
            }
            if (j.contains("team")) {
                result.team_id = j["team"].value("id", "");
                result.team_name = j["team"].value("name", "");
            }
            LOG_INFO("OAuth success! team=" + result.team_name +
                     " user=" + result.user_id +
                     " token=" + result.access_token.substr(0, 10) + "...");
        } else {
            result.error = j.value("error", "unknown");
            LOG_ERROR("OAuth token exchange failed: " + result.error);
        }
    } catch (const std::exception& e) {
        result.error = std::string("failed to parse OAuth response: ") + e.what();
        LOG_ERROR(result.error);
    }

    return result;
}

void OAuthFlow::start() {
    complete_ = false;
    std::thread([this]() {
        result_ = execute();
        complete_ = true;
    }).detach();
}

} // namespace conduit
