#include "slack/SlackAuth.h"
#include "util/Logger.h"

namespace conduit::slack {

AuthInfo SlackAuth::test(WebAPI& api) {
    AuthInfo info;
    auto result = api.get("auth.test");

    if (!result) {
        LOG_ERROR("auth.test failed - couldn't reach slack");
        return info;
    }

    auto& j = *result;
    info.ok = j.value("ok", false);

    if (info.ok) {
        info.user_id = j.value("user_id", "");
        info.team_id = j.value("team_id", "");
        info.team_name = j.value("team", "");
        info.user_name = j.value("user", "");
        info.url = j.value("url", "");
        LOG_INFO("authenticated as " + info.user_name + " in " + info.team_name);
    } else {
        std::string error = j.value("error", "unknown");
        LOG_ERROR("auth.test failed: " + error);
    }

    return info;
}

bool SlackAuth::isValidTokenFormat(const std::string& token) {
    // slack tokens have predictable prefixes
    return token.starts_with("xoxp-") || token.starts_with("xoxb-") ||
           token.starts_with("xapp-") || token.starts_with("xoxe-");
}

std::string SlackAuth::redact(const std::string& token) {
    if (token.size() < 10) return "***";
    return token.substr(0, 5) + "..." + token.substr(token.size() - 4);
}

} // namespace conduit::slack
