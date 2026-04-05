#include "app/SlackTokenFinder.h"
#include "util/Logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <algorithm>

#include <sqlite3.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")
#endif

namespace fs = std::filesystem;

namespace conduit {

// ---- directory discovery ----

std::vector<std::string> SlackTokenFinder::findSlackDirs() {
    std::vector<std::string> dirs;

#ifdef _WIN32
    // the normal install lives in %APPDATA%/Slack
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        std::string slack_dir = std::string(appdata) + "/Slack";
        if (fs::exists(slack_dir)) {
            dirs.push_back(slack_dir);
            LOG_DEBUG("found Slack at " + slack_dir);
        }
    }

    // MS Store version buries itself under a GUID-laden wasteland
    const char* localappdata = std::getenv("LOCALAPPDATA");
    if (localappdata) {
        std::string packages = std::string(localappdata) + "/Packages";
        std::error_code ec;
        if (fs::exists(packages, ec)) {
            for (auto& entry : fs::directory_iterator(packages, ec)) {
                if (!entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                if (name.find("com.tinyspeck.slackdesktop") != std::string::npos) {
                    std::string store_dir = entry.path().string() +
                                            "/LocalCache/Roaming/Slack";
                    if (fs::exists(store_dir, ec)) {
                        dirs.push_back(store_dir);
                        LOG_DEBUG("found MS Store Slack at " + store_dir);
                    }
                }
            }
        }
    }
#endif

    if (dirs.empty()) {
        LOG_DEBUG("no Slack installations found (are you even a real developer?)");
    }

    return dirs;
}

// ---- LevelDB token extraction ----
//
// LevelDB is a pain to parse properly, but we don't need to. the tokens are
// stored as string values in .ldb files and we can just grep for them like
// animals. the format is: xoxc-{digits}-{digits}-{digits}-{64 hex chars}

static bool isTokenChar(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F') ||
           c == '-';
}

static bool validateXoxcToken(const std::string& token) {
    // should look like xoxc-NNNNN-NNNNN-NNNNN-<64 hex>
    // total length is typically 70-90 chars
    if (token.size() < 50 || token.size() > 120) return false;
    if (token.substr(0, 5) != "xoxc-") return false;

    // quick sanity: should have at least 3 dashes after the prefix
    int dashes = 0;
    for (size_t i = 5; i < token.size(); i++) {
        if (token[i] == '-') dashes++;
    }
    return dashes >= 3;
}

std::vector<std::string> SlackTokenFinder::extractTokensFromLevelDB(const std::string& leveldb_dir) {
    std::vector<std::string> tokens;

    std::error_code ec;
    if (!fs::exists(leveldb_dir, ec)) return tokens;

    for (auto& entry : fs::directory_iterator(leveldb_dir, ec)) {
        std::string ext = entry.path().extension().string();
        // .ldb files are the SSTable data files, .log is the write-ahead log
        if (ext != ".ldb" && ext != ".log") continue;

        // slurp the whole file. these are usually small (< 10MB).
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file.is_open()) continue;

        std::string data((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        file.close();

        // scan for the magic prefix
        size_t pos = 0;
        while ((pos = data.find("xoxc-", pos)) != std::string::npos) {
            // assemble the token, skipping over LevelDB's null bytes and
            // control characters that get sprinkled through record boundaries
            std::string token = "xoxc-";
            size_t i = pos + 5;
            int garbage_run = 0;

            while (i < data.size() && token.size() < 120) {
                char c = data[i];
                if (isTokenChar(c)) {
                    token += c;
                    garbage_run = 0;
                } else if (c >= 0x00 && c <= 0x1F) {
                    // control char / record separator - skip it but track
                    // consecutive garbage so we know when the token ends
                    garbage_run++;
                    if (garbage_run > 4) break; // too much junk, we're past the token
                } else {
                    // non-token, non-control char means we've hit the end
                    break;
                }
                i++;
            }

            if (validateXoxcToken(token)) {
                // dedup - LevelDB keeps old versions around like a hoarder
                if (std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
                    tokens.push_back(token);
                    LOG_DEBUG("found xoxc token in " + entry.path().filename().string() +
                              " (length " + std::to_string(token.size()) + ")");
                }
            }

            pos = i;
        }
    }

    return tokens;
}

// ---- Cookie extraction ----
//
// Slack stores the session cookie in a SQLite database, encrypted with DPAPI on
// Windows. DPAPI decryption only works for the same user account that encrypted
// it, which is fine because we ARE that user. thanks, Windows.

std::string SlackTokenFinder::extractDCookie(const std::string& cookies_path) {
    std::error_code ec;
    if (!fs::exists(cookies_path, ec)) {
        LOG_DEBUG("no cookies DB at " + cookies_path);
        return "";
    }

    // Slack keeps the DB locked while running (because of course it does).
    // copy it to a temp file so we can read it without fighting over file handles.
    std::string temp_path = cookies_path + ".conduit_tmp";
    try {
        fs::copy_file(cookies_path, temp_path,
                       fs::copy_options::overwrite_existing, ec);
        if (ec) {
            LOG_WARN("couldn't copy cookies DB: " + ec.message());
            return "";
        }
    } catch (const std::exception& e) {
        LOG_WARN(std::string("cookies DB copy failed: ") + e.what());
        return "";
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(temp_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        LOG_WARN("couldn't open cookies DB: " + std::string(sqlite3_errmsg(db)));
        sqlite3_close(db);
        fs::remove(temp_path, ec);
        return "";
    }

    std::string decrypted_cookie;

    // the cookie we want is 'd' on .slack.com
    const char* sql = "SELECT encrypted_value FROM cookies "
                      "WHERE host_key = '.slack.com' AND name = 'd' LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        if (blob && blob_size > 0) {
#ifdef _WIN32
            // DPAPI decrypt - the encrypted blob starts with a version prefix
            // that we need to handle. Chromium-based apps (which Slack is,
            // because everything is Electron these days) prepend "v10" or "v20".
            const unsigned char* raw = static_cast<const unsigned char*>(blob);
            int offset = 0;

            // skip the version prefix if present
            if (blob_size > 3 && raw[0] == 'v' && raw[1] == '1' && raw[2] == '0') {
                offset = 3;
            }

            DATA_BLOB encrypted_blob;
            encrypted_blob.pbData = const_cast<BYTE*>(raw + offset);
            encrypted_blob.cbData = blob_size - offset;

            DATA_BLOB decrypted_blob;
            if (CryptUnprotectData(&encrypted_blob, nullptr, nullptr, nullptr,
                                   nullptr, 0, &decrypted_blob)) {
                decrypted_cookie = std::string(
                    reinterpret_cast<char*>(decrypted_blob.pbData),
                    decrypted_blob.cbData);
                LocalFree(decrypted_blob.pbData);
                LOG_DEBUG("decrypted d cookie (" +
                          std::to_string(decrypted_cookie.size()) + " bytes)");
            } else {
                LOG_WARN("DPAPI decrypt failed for d cookie (error " +
                         std::to_string(GetLastError()) + ")");
            }
#else
            // on mac/linux this would be keychain/kwallet decryption
            // which is a whole different can of worms. later.
            LOG_WARN("cookie decryption not implemented for this platform yet");
#endif
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    fs::remove(temp_path, ec);

    return decrypted_cookie;
}

// ---- tie it all together ----

std::vector<SlackTokenSet> SlackTokenFinder::findTokens() {
    std::vector<SlackTokenSet> results;

    auto slack_dirs = findSlackDirs();
    if (slack_dirs.empty()) return results;

    for (auto& dir : slack_dirs) {
        // LevelDB stores live here
        std::string leveldb_dir = dir + "/Local Storage/leveldb";
        auto tokens = extractTokensFromLevelDB(leveldb_dir);

        // cookies DB lives here
        std::string cookies_path = dir + "/Network/Cookies";
        std::string cookie = extractDCookie(cookies_path);

        // pair each token with the cookie. in theory there could be
        // multiple workspace tokens but they all share the same session cookie.
        for (auto& token : tokens) {
            SlackTokenSet ts;
            ts.xoxc_token = token;
            ts.d_cookie = cookie;
            // we could try to extract the team name from LevelDB too but
            // that's more trouble than it's worth right now
            results.push_back(std::move(ts));
        }
    }

    if (results.empty()) {
        LOG_DEBUG("searched " + std::to_string(slack_dirs.size()) +
                  " Slack dir(s) but came up empty. log in to Slack desktop first.");
    } else {
        LOG_INFO("found " + std::to_string(results.size()) + " Slack token(s)");
    }

    return results;
}

} // namespace conduit
