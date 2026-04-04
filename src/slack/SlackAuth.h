#pragma once
#include "slack/WebAPI.h"
#include "slack/Types.h"
#include <string>

namespace conduit::slack {

struct AuthInfo {
    UserId user_id;
    TeamId team_id;
    std::string team_name;
    std::string user_name;
    std::string url;
    bool ok = false;
};

class SlackAuth {
public:
    // validate a token with auth.test and return the info
    static AuthInfo test(WebAPI& api);

    // check if a token looks valid (basic format check, not a network call)
    static bool isValidTokenFormat(const std::string& token);

    // redact a token for logging (because logging tokens is bad, kids)
    static std::string redact(const std::string& token);
};

} // namespace conduit::slack
