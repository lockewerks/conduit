#include "app/BrowserCredentials.h"
#include "util/Logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Bcrypt.lib")
#endif

#include <sqlite3.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// base64 decode (for Chrome's Local State encryption key)
static std::vector<uint8_t> base64Decode(const std::string& input) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}

namespace conduit {

std::vector<BrowserCredentials::BrowserProfile> BrowserCredentials::findChromiumProfiles() {
    std::vector<BrowserProfile> profiles;

#ifdef _WIN32
    std::string local_app = "";
    char* env = std::getenv("LOCALAPPDATA");
    if (env) local_app = env;
    if (local_app.empty()) return profiles;

    // every chromium browser and its cousin
    struct { const char* name; const char* rel_path; } browsers[] = {
        {"Chrome",     "Google\\Chrome\\User Data"},
        {"Edge",       "Microsoft\\Edge\\User Data"},
        {"Brave",      "BraveSoftware\\Brave-Browser\\User Data"},
        {"Vivaldi",    "Vivaldi\\User Data"},
        {"Opera GX",   "Opera Software\\Opera GX Stable"},
        {"Opera",      "Opera Software\\Opera Stable"},
        {"Chromium",   "Chromium\\User Data"},
    };

    for (auto& [name, rel] : browsers) {
        std::string path = local_app + "\\" + rel;
        if (fs::exists(path) && fs::is_directory(path)) {
            profiles.push_back({name, path});
            LOG_DEBUG("found browser: " + std::string(name) + " at " + path);
        }
    }
#elif defined(__APPLE__)
    std::string home = "";
    char* env = std::getenv("HOME");
    if (env) home = env;
    if (home.empty()) return profiles;

    struct { const char* name; const char* rel_path; } browsers[] = {
        {"Chrome",  "Library/Application Support/Google/Chrome"},
        {"Edge",    "Library/Application Support/Microsoft Edge"},
        {"Brave",   "Library/Application Support/BraveSoftware/Brave-Browser"},
        {"Vivaldi", "Library/Application Support/Vivaldi"},
    };
    for (auto& [name, rel] : browsers) {
        std::string path = home + "/" + rel;
        if (fs::exists(path)) profiles.push_back({name, path});
    }
#else
    std::string home = "";
    char* env = std::getenv("HOME");
    if (env) home = env;
    if (home.empty()) return profiles;

    struct { const char* name; const char* rel_path; } browsers[] = {
        {"Chrome",   ".config/google-chrome"},
        {"Chromium", ".config/chromium"},
        {"Edge",     ".config/microsoft-edge"},
        {"Brave",    ".config/BraveSoftware/Brave-Browser"},
        {"Vivaldi",  ".config/vivaldi"},
    };
    for (auto& [name, rel] : browsers) {
        std::string path = home + "/" + rel;
        if (fs::exists(path)) profiles.push_back({name, path});
    }
#endif

    return profiles;
}

std::optional<std::string> BrowserCredentials::extractToken(const std::string& local_storage_path) {
    // scan .ldb and .log files in the leveldb directory for the xoxc- token.
    // levelDB files contain raw key-value data — the token is stored as part
    // of the localConfig_v2 JSON blob, unencrypted, in plaintext. thanks google.
    try {
        if (!fs::exists(local_storage_path) || !fs::is_directory(local_storage_path))
            return std::nullopt;

        for (auto& entry : fs::directory_iterator(local_storage_path)) {
            try {
                std::string ext = entry.path().extension().string();
                if (ext != ".ldb" && ext != ".log") continue;

                std::ifstream file(entry.path(), std::ios::binary);
                if (!file.is_open()) continue;

                std::string content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());

                size_t pos = content.find("xoxc-");
                if (pos == std::string::npos) continue;

                size_t end = pos;
                while (end < content.size()) {
                    char c = content[end];
                    if (std::isalnum(c) || c == '-') end++;
                    else break;
                }

                std::string token = content.substr(pos, end - pos);
                if (token.size() > 20) {
                    LOG_INFO("found Slack token in browser localStorage (" +
                             std::to_string(token.size()) + " chars)");
                    return token;
                }
            } catch (...) {
                continue; // skip files we can't read
            }
        }
    } catch (...) {
        // directory iteration failed (permissions, locked files, etc)
    }

    return std::nullopt;
}

#ifdef _WIN32

std::optional<std::vector<uint8_t>> BrowserCredentials::getEncryptionKey(
    const std::string& user_data_path) {
    // Chrome stores its encryption key in Local State, base64-encoded,
    // wrapped in DPAPI. we unwrap it to get the raw AES-256-GCM key.
    std::string local_state_path = user_data_path + "\\Local State";
    if (!fs::exists(local_state_path)) return std::nullopt;

    std::ifstream file(local_state_path);
    if (!file.is_open()) return std::nullopt;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    try {
        auto j = nlohmann::json::parse(content);
        std::string key_b64 = j["os_crypt"]["encrypted_key"].get<std::string>();
        auto key_enc = base64Decode(key_b64);

        // strip the "DPAPI" prefix (5 bytes)
        if (key_enc.size() < 5) return std::nullopt;
        key_enc.erase(key_enc.begin(), key_enc.begin() + 5);

        // decrypt with DPAPI
        DATA_BLOB in_blob, out_blob;
        in_blob.pbData = key_enc.data();
        in_blob.cbData = (DWORD)key_enc.size();

        if (!CryptUnprotectData(&in_blob, nullptr, nullptr, nullptr, nullptr, 0, &out_blob)) {
            LOG_ERROR("DPAPI decrypt failed for browser encryption key");
            return std::nullopt;
        }

        std::vector<uint8_t> key(out_blob.pbData, out_blob.pbData + out_blob.cbData);
        LocalFree(out_blob.pbData);
        return key;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> BrowserCredentials::decryptChromeValue(
    const std::vector<uint8_t>& encrypted, const std::string& user_data_path) {
    if (encrypted.size() < 15) return std::nullopt;

    // v10/v11 uses DPAPI directly (no prefix or "v10" prefix)
    // v20 uses AES-256-GCM with a key from Local State ("v20" prefix)
    // check for "v10" or "v20" prefix
    std::string prefix(encrypted.begin(), encrypted.begin() + 3);

    if (prefix == "v20" || prefix == "v10") {
        // AES-256-GCM: prefix(3) + nonce(12) + ciphertext + tag(16)
        auto key_opt = getEncryptionKey(user_data_path);
        if (!key_opt) return std::nullopt;
        auto& key = *key_opt;

        const uint8_t* nonce = encrypted.data() + 3;
        int nonce_len = 12;
        const uint8_t* ciphertext = encrypted.data() + 3 + nonce_len;
        int ct_len = (int)encrypted.size() - 3 - nonce_len - 16; // subtract tag
        if (ct_len <= 0) return std::nullopt;

        // use BCrypt for AES-GCM decryption
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_KEY_HANDLE hKey = nullptr;
        NTSTATUS status;

        status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (status != 0) return std::nullopt;

        status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                                    (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                                    sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (status != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); return std::nullopt; }

        status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                             (PUCHAR)key.data(), (ULONG)key.size(), 0);
        if (status != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); return std::nullopt; }

        // set up the auth info
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)nonce;
        authInfo.cbNonce = nonce_len;
        authInfo.pbTag = (PUCHAR)(encrypted.data() + encrypted.size() - 16);
        authInfo.cbTag = 16;

        std::vector<uint8_t> plaintext(ct_len);
        ULONG bytes_written = 0;

        status = BCryptDecrypt(hKey, (PUCHAR)ciphertext, ct_len,
                                &authInfo, nullptr, 0,
                                plaintext.data(), ct_len, &bytes_written, 0);

        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        if (status != 0) return std::nullopt;

        return std::string(plaintext.begin(), plaintext.begin() + bytes_written);
    }

    // fallback: try raw DPAPI (older Chrome versions)
    DATA_BLOB in_blob, out_blob;
    in_blob.pbData = (BYTE*)encrypted.data();
    in_blob.cbData = (DWORD)encrypted.size();

    if (CryptUnprotectData(&in_blob, nullptr, nullptr, nullptr, nullptr, 0, &out_blob)) {
        std::string result((char*)out_blob.pbData, out_blob.cbData);
        LocalFree(out_blob.pbData);
        return result;
    }

    return std::nullopt;
}

#endif // _WIN32

std::optional<std::string> BrowserCredentials::extractCookie(const std::string& user_data_path) {
#ifdef _WIN32
    try {
    // Chrome locks the Cookies file while running. copy it to a temp location.
    std::vector<std::string> cookie_paths = {
        user_data_path + "\\Default\\Network\\Cookies",
        user_data_path + "\\Default\\Cookies",
        user_data_path + "\\Profile 1\\Network\\Cookies",
        user_data_path + "\\Profile 1\\Cookies",
    };

    std::string src_path;
    for (auto& p : cookie_paths) {
        if (fs::exists(p)) { src_path = p; break; }
    }
    if (src_path.empty()) return std::nullopt;

    // copy to temp so we don't fight Chrome for the file lock
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string tmp_path = std::string(tmp) + "conduit_cookies_" +
                            std::to_string(GetTickCount()) + ".db";
    try {
        fs::copy_file(src_path, tmp_path, fs::copy_options::overwrite_existing);
    } catch (...) {
        LOG_WARN("couldn't copy cookie DB (browser might be locking it)");
        return std::nullopt;
    }

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(tmp_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        fs::remove(tmp_path);
        return std::nullopt;
    }

    std::optional<std::string> result;
    const char* sql = "SELECT encrypted_value FROM cookies "
                      "WHERE host_key = '.slack.com' AND name = 'd' "
                      "ORDER BY last_access_utc DESC LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blob_size = sqlite3_column_bytes(stmt, 0);
            if (blob && blob_size > 0) {
                std::vector<uint8_t> encrypted((uint8_t*)blob, (uint8_t*)blob + blob_size);
                result = decryptChromeValue(encrypted, user_data_path);
                if (result) {
                    LOG_INFO("decrypted Slack d= cookie from browser (" +
                             std::to_string(result->size()) + " chars)");
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    fs::remove(tmp_path);
    return result;

    } catch (const std::exception& e) {
        LOG_WARN("cookie extraction failed: " + std::string(e.what()));
        return std::nullopt;
    } catch (...) {
        LOG_WARN("cookie extraction failed with unknown error");
        return std::nullopt;
    }

#else
    // macOS/Linux: cookie decryption is more involved (keychain/kwallet)
    // punt for now — users paste the cookie manually on non-Windows
    return std::nullopt;
#endif
}

std::vector<BrowserCredential> BrowserCredentials::scan() {
    std::vector<BrowserCredential> results;

    try {
        auto profiles = findChromiumProfiles();

        for (auto& [name, path] : profiles) {
            try {
                LOG_INFO("scanning " + name + " for Slack credentials...");

                std::vector<std::string> profile_dirs = {
                    "Default", "Profile 1", "Profile 2", "Profile 3"
                };

                for (auto& profile : profile_dirs) {
                    std::string ls_path = path +
#ifdef _WIN32
                        "\\" + profile + "\\Local Storage\\leveldb";
#else
                        "/" + profile + "/Local Storage/leveldb";
#endif

                    auto token = extractToken(ls_path);
                    if (!token) continue;

                    auto cookie = extractCookie(path);

                    results.push_back({name, *token, cookie.value_or("")});
                    LOG_INFO("got credentials from " + name + " (" + profile + ")");
                    break;
                }
            } catch (const std::exception& e) {
                LOG_WARN("error scanning " + name + ": " + e.what());
            } catch (...) {
                LOG_WARN("unknown error scanning " + name);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("browser scan failed: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("browser scan failed with unknown error");
    }

    return results;
}

} // namespace conduit
