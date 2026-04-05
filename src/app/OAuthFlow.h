#pragma once
#include <string>

namespace conduit {

struct OAuthResult {
    std::string access_token;
    std::string team_id;
    std::string team_name;
    std::string user_id;
    bool ok = false;
    std::string error;
};

// OAuth 2.0 for Slack, copy-paste style.
// opens the browser, user approves, copies the code, pastes it in conduit.
// we exchange the code for a token. no localhost server, no HTTPS, no bullshit.
class OAuthFlow {
public:
    OAuthFlow(const std::string& client_id, const std::string& client_secret);

    // open the browser to slack's authorize page
    void openBrowser();

    // exchange an authorization code for a token
    OAuthResult exchangeCode(const std::string& code);

    // the URL the user needs to visit (in case openBrowser fails)
    std::string authorizeURL() const { return auth_url_; }

private:
    std::string client_id_;
    std::string client_secret_;
    std::string auth_url_;
};

} // namespace conduit
