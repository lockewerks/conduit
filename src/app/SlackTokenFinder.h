#pragma once
#include <string>
#include <optional>
#include <vector>

namespace conduit {

struct SlackTokenSet {
    std::string xoxc_token;  // the xoxc- user token from LevelDB
    std::string d_cookie;     // the d= session cookie (decrypted)
    std::string team_name;    // workspace name if we can find it
};

// raids the local Slack desktop install for auth tokens.
// morally questionable? maybe. but configuring OAuth apps is worse.
class SlackTokenFinder {
public:
    // scan the local Slack desktop app for tokens
    // returns all workspace tokens found
    static std::vector<SlackTokenSet> findTokens();

private:
    // find all possible Slack data directories
    static std::vector<std::string> findSlackDirs();

    // scan LevelDB files for xoxc- tokens
    static std::vector<std::string> extractTokensFromLevelDB(const std::string& leveldb_dir);

    // extract and decrypt the d cookie from Cookies SQLite DB
    static std::string extractDCookie(const std::string& cookies_path);
};

} // namespace conduit
