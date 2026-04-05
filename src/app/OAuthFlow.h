#pragma once
#include <string>
#include <optional>
#include <functional>

namespace conduit {

struct OAuthResult {
    std::string access_token;  // xoxp- user token
    std::string team_id;
    std::string team_name;
    std::string user_id;
    bool ok = false;
    std::string error;
};

// handles the full OAuth 2.0 flow for Slack:
// 1. spin up a tiny HTTP server on localhost
// 2. open the browser to slack's authorize page
// 3. catch the redirect with the auth code
// 4. exchange the code for a token
// 5. profit
class OAuthFlow {
public:
    OAuthFlow(const std::string& client_id, const std::string& client_secret);

    // kick off the flow. blocks until the user completes auth or timeout.
    // returns the token or an error.
    OAuthResult execute();

    // non-blocking version: start the flow, call poll() each frame
    void start();
    bool isComplete() const { return complete_; }
    OAuthResult result() const { return result_; }

private:
    std::string client_id_;
    std::string client_secret_;
    int port_ = 0;
    bool complete_ = false;
    OAuthResult result_;

    // find an available port for the localhost callback
    int findAvailablePort();

    // exchange the auth code for an access token
    OAuthResult exchangeCode(const std::string& code, const std::string& redirect_uri);
};

} // namespace conduit
