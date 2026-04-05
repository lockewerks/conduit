#include "app/OAuthFlow.h"
#include "util/Logger.h"
#include "util/Platform.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace conduit {

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
    : client_id_(client_id), client_secret_(client_secret) {
    // no redirect_uri — slack will show the code on its own page
    auth_url_ = "https://slack.com/oauth/v2/authorize?"
                "client_id=" + client_id_ +
                "&user_scope=" + OAUTH_SCOPES;
}

void OAuthFlow::openBrowser() {
    LOG_INFO("opening browser for Slack OAuth");
    platform::openURL(auth_url_);
}

OAuthResult OAuthFlow::exchangeCode(const std::string& code) {
    OAuthResult result;

    if (code.empty()) {
        result.error = "empty authorization code";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl init failed";
        return result;
    }

    std::string post_data = "client_id=" + client_id_ +
                            "&client_secret=" + client_secret_ +
                            "&code=" + code;

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
            if (j.contains("authed_user")) {
                result.access_token = j["authed_user"].value("access_token", "");
                result.user_id = j["authed_user"].value("id", "");
            }
            if (j.contains("team")) {
                result.team_id = j["team"].value("id", "");
                result.team_name = j["team"].value("name", "");
            }
            LOG_INFO("OAuth success: " + result.team_name + " / " + result.user_id);
        } else {
            result.error = j.value("error", "unknown");
            LOG_ERROR("OAuth token exchange failed: " + result.error);
        }
    } catch (const std::exception& e) {
        result.error = std::string("failed to parse response: ") + e.what();
    }

    return result;
}

} // namespace conduit
