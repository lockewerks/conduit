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

#ifdef CONDUIT_HAS_LEVELDB
#include <leveldb/db.h>
#include <leveldb/options.h>
#endif

namespace fs = std::filesystem;

// LevelDB stores data blocks with Snappy compression. Snappy is a simple
// format: the decompressed length as a varint, then a series of literals
// and copy commands. we need to decompress to get the actual JSON strings
// because the compression back-references fragment tokens across blocks.
static std::string snappyDecompress(const uint8_t* data, size_t len) {
    if (len == 0) return "";
    size_t pos = 0;

    // read decompressed length as varint
    uint32_t dec_len = 0;
    int shift = 0;
    while (pos < len) {
        uint8_t b = data[pos++];
        dec_len |= (uint32_t)(b & 0x7f) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }

    if (dec_len > 64 * 1024 * 1024) return ""; // sanity check: 64MB max
    std::string output;
    output.reserve(dec_len);

    while (pos < len && output.size() < dec_len) {
        uint8_t tag = data[pos++];
        int type = tag & 0x03;

        if (type == 0) {
            // literal
            uint32_t lit_len = (tag >> 2) + 1;
            if (lit_len == 61 && pos < len) {
                lit_len = data[pos++] + 1;
            } else if (lit_len == 62 && pos + 1 < len) {
                lit_len = data[pos] | (data[pos+1] << 8);
                pos += 2;
                lit_len += 1;
            } else if (lit_len == 63 && pos + 2 < len) {
                lit_len = data[pos] | (data[pos+1] << 8) | (data[pos+2] << 16);
                pos += 3;
                lit_len += 1;
            } else if (lit_len == 64 && pos + 3 < len) {
                lit_len = data[pos] | (data[pos+1] << 8) | (data[pos+2] << 16) | (data[pos+3] << 24);
                pos += 4;
                lit_len += 1;
            }
            if (pos + lit_len > len) break;
            output.append(reinterpret_cast<const char*>(data + pos), lit_len);
            pos += lit_len;
        } else if (type == 1) {
            // copy with 1-byte offset
            uint32_t copy_len = ((tag >> 2) & 0x07) + 4;
            if (pos >= len) break;
            uint32_t offset = ((tag >> 5) << 8) | data[pos++];
            if (offset == 0 || offset > output.size()) break;
            for (uint32_t i = 0; i < copy_len; i++) {
                output += output[output.size() - offset];
            }
        } else if (type == 2) {
            // copy with 2-byte offset
            uint32_t copy_len = (tag >> 2) + 1;
            if (pos + 1 >= len) break;
            uint32_t offset = data[pos] | (data[pos+1] << 8);
            pos += 2;
            if (offset == 0 || offset > output.size()) break;
            for (uint32_t i = 0; i < copy_len; i++) {
                output += output[output.size() - offset];
            }
        } else {
            // copy with 4-byte offset
            uint32_t copy_len = (tag >> 2) + 1;
            if (pos + 3 >= len) break;
            uint32_t offset = data[pos] | (data[pos+1] << 8) | (data[pos+2] << 16) | (data[pos+3] << 24);
            pos += 4;
            if (offset == 0 || offset > output.size()) break;
            for (uint32_t i = 0; i < copy_len; i++) {
                output += output[output.size() - offset];
            }
        }
    }

    return output;
}

// strip LevelDB block framing from a .ldb file and decompress each block.
// LevelDB .ldb (sstable) files have data blocks followed by index/meta blocks.
// each block ends with a 1-byte compression type + 4-byte CRC.
// we try to decompress each data region that looks like it might contain our JSON.
static std::string decompressLevelDBFile(const std::string& content) {
    // simple approach: find the localConfig_v2 region in the raw bytes,
    // then try to Snappy-decompress overlapping chunks around it.
    // LevelDB data blocks are typically 4KB and start with key-value pairs.

    // first, try treating the entire file region around localConfig_v2
    // as Snappy-compressed data. find the block that contains it.
    size_t config_pos = content.find("ocalConfig_v2");
    if (config_pos == std::string::npos) return "";

    // scan backward from config_pos to find the block start.
    // LevelDB blocks are aligned and typically start with a restart array.
    // the simplest approach: try decompressing starting from various positions
    // before the config marker.

    // actually, let's just extract printable strings and reassemble.
    // the Snappy back-references copy from earlier in the output,
    // so we need proper decompression. try decompressing from the
    // start of the data region.

    // LevelDB .ldb files: the first data block starts right at the beginning.
    // block format: [entries][restart_points][num_restarts][compression_type][crc32]
    // for a Snappy-compressed block, compression_type = 0x01

    // try to find Snappy blocks by looking for valid Snappy headers.
    // a Snappy compressed block starts with the uncompressed length as a varint.
    std::string result;

    // try decompressing the chunk that contains our data
    // scan for potential Snappy block starts near the config data
    for (size_t try_start = (config_pos > 8192) ? config_pos - 8192 : 0;
         try_start < config_pos && try_start < content.size(); try_start += 64) {

        std::string decompressed = snappyDecompress(
            reinterpret_cast<const uint8_t*>(content.data() + try_start),
            std::min(content.size() - try_start, (size_t)131072));

        if (decompressed.size() > 100 && decompressed.find("\"teams\"") != std::string::npos) {
            if (decompressed.size() > result.size()) {
                result = decompressed;
            }
        }
    }

    return result;
}

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

std::vector<BrowserCredential> BrowserCredentials::extractTeams(
    const std::string& local_storage_path, const std::string& browser_name) {
    // Chrome's localStorage LevelDB uses Snappy block compression.
    // the localConfig_v2 JSON (containing per-team xoxc tokens) is stored
    // in compressed SSTable blocks. we find it by:
    // 1. scanning raw .ldb file bytes for "ocalConfig_v2" marker
    // 2. brute-force trying Snappy decompression from offsets before the marker
    // 3. parsing the clean decompressed JSON to extract tokens and team names
    std::vector<BrowserCredential> results;

    try {
        if (!fs::exists(local_storage_path) || !fs::is_directory(local_storage_path))
            return results;

        // scan .ldb files for the localConfig_v2 data
        // scan backward for the Snappy block boundary and decompress.
#ifdef CONDUIT_HAS_LEVELDB_DISABLED_USE_SNAPPY_INSTEAD
        {
            // copy the entire LevelDB directory to a temp location so we don't
            // fight the browser for the LOCK file. this also ensures we get a
            // consistent snapshot even if the browser is writing.
            char tmp_buf[MAX_PATH];
            GetTempPathA(MAX_PATH, tmp_buf);
            std::string tmp_ldb = std::string(tmp_buf) + "conduit_ldb_" +
                                   std::to_string(GetTickCount());
            bool copied = false;
            try {
                fs::copy(local_storage_path, tmp_ldb, fs::copy_options::recursive |
                         fs::copy_options::overwrite_existing);
                // remove the LOCK file from the copy so LevelDB can open it
                fs::remove(tmp_ldb + "/LOCK");
                copied = true;
                LOG_INFO("copied LevelDB to temp: " + tmp_ldb);
            } catch (const std::exception& e) {
                LOG_WARN("failed to copy LevelDB dir: " + std::string(e.what()));
            }

            std::string db_path = copied ? tmp_ldb : local_storage_path;

            leveldb::DB* db = nullptr;
            leveldb::Options opts;
            opts.create_if_missing = false;
            auto status = leveldb::DB::Open(opts, db_path, &db);
            if (status.ok() && db) {
                LOG_INFO("opened LevelDB at " + local_storage_path);

                // iterate all keys looking for localConfig_v2 or xoxc tokens.
                // Chrome localStorage keys have binary prefixes like:
                //   _https://app.slack.com\x00\x01localConfig_v2
                // so we search both key and value for our markers.
                leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
                int key_count = 0;
                for (it->SeekToFirst(); it->Valid(); it->Next()) {
                    key_count++;
                    // dump first 20 keys for debugging
                    if (key_count <= 20) {
                        std::string dk = it->key().ToString();
                        std::string safe;
                        for (size_t di = 0; di < std::min(dk.size(), (size_t)120); di++) {
                            unsigned char dc = static_cast<unsigned char>(dk[di]);
                            if (dc >= 0x20 && dc < 0x7f) safe += static_cast<char>(dc);
                            else { safe += "\\x"; safe += "0123456789abcdef"[dc>>4]; safe += "0123456789abcdef"[dc&0xf]; }
                        }
                        LOG_INFO("  key[" + std::to_string(key_count) + "]: " + safe +
                                 " (val:" + std::to_string(it->value().size()) + ")");
                    }
                    std::string key = it->key().ToString();
                    std::string value = it->value().ToString();

                    // Chrome stores localStorage keys as:
                    //   META: or _origin\x00\x01key
                    // where the key portion may be UTF-16LE encoded.
                    // check both the raw key bytes and a UTF-16 decoded version.

                    // build a printable version of the key for matching
                    std::string ascii_key;
                    for (size_t ki = 0; ki < key.size(); ki++) {
                        unsigned char c = static_cast<unsigned char>(key[ki]);
                        if (c >= 0x20 && c < 0x7f) ascii_key += static_cast<char>(c);
                    }

                    // also try UTF-16LE decode (skip null bytes between ASCII chars)
                    std::string utf16_key;
                    for (size_t ki = 0; ki + 1 < key.size(); ki += 2) {
                        unsigned char lo = static_cast<unsigned char>(key[ki]);
                        unsigned char hi = static_cast<unsigned char>(key[ki + 1]);
                        if (hi == 0 && lo >= 0x20 && lo < 0x7f) {
                            utf16_key += static_cast<char>(lo);
                        }
                    }

                    bool key_match = ascii_key.find("localConfig_v2") != std::string::npos ||
                                     utf16_key.find("localConfig_v2") != std::string::npos;
                    bool value_match = value.find("xoxc-") != std::string::npos ||
                                       value.find("{\"teams\"") != std::string::npos;

                    // also check value as UTF-16LE
                    std::string utf16_value;
                    if (!value_match && value.size() > 10) {
                        for (size_t vi = 0; vi + 1 < value.size(); vi += 2) {
                            unsigned char lo = static_cast<unsigned char>(value[vi]);
                            unsigned char hi = static_cast<unsigned char>(value[vi + 1]);
                            if (hi == 0 && lo >= 0x20 && lo < 0x7f) {
                                utf16_value += static_cast<char>(lo);
                            }
                        }
                        value_match = utf16_value.find("xoxc-") != std::string::npos ||
                                       utf16_value.find("{\"teams\"") != std::string::npos;
                        if (value_match) {
                            // use the decoded UTF-16 value for JSON parsing
                            value = utf16_value;
                        }
                    }

                    if (!key_match && !value_match) continue;

                    LOG_INFO("found matching LevelDB entry: key=" + ascii_key.substr(0, 80) +
                             " (value: " + std::to_string(value.size()) + " bytes)");

                    // the value might have a 1-byte prefix (type indicator) before the JSON
                    size_t json_start = value.find('{');
                    if (json_start == std::string::npos) continue;

                    try {
                        auto config = nlohmann::json::parse(
                            value.begin() + json_start, value.end());

                        if (!config.contains("teams") || !config["teams"].is_object()) continue;

                        for (auto& [team_id, team_data] : config["teams"].items()) {
                            if (!team_data.is_object()) continue;
                            std::string token = team_data.value("token", "");
                            if (token.find("xoxc-") != 0) continue;

                            BrowserCredential cred;
                            cred.browser_name = browser_name;
                            cred.token = token;
                            cred.team_id = team_id;
                            cred.team_name = team_data.value("name", team_id);
                            cred.team_url = team_data.value("url", "");
                            cred.team_domain = team_data.value("domain", "");

                            LOG_INFO("found team via LevelDB: " + cred.team_name +
                                     " (" + cred.team_id + ") token_len=" +
                                     std::to_string(cred.token.size()));
                            results.push_back(std::move(cred));
                        }
                    } catch (const std::exception& e) {
                        LOG_WARN("JSON parse error: " + std::string(e.what()));
                    }
                }
                LOG_INFO("scanned " + std::to_string(key_count) + " LevelDB keys, found " +
                         std::to_string(results.size()) + " teams");
                delete it;
                delete db;

                // clean up temp copy
                if (copied) {
                    try { fs::remove_all(tmp_ldb); } catch (...) {}
                }

                if (!results.empty()) return results;
            } else {
                LOG_WARN("LevelDB open failed: " + status.ToString());
                // clean up temp copy on failure too
                if (copied) {
                    try { fs::remove_all(tmp_ldb); } catch (...) {}
                }
            }
        }
#endif

        // scan LevelDB .ldb files, find the Snappy block containing
        // localConfig_v2, decompress it, and parse the clean JSON.
        for (auto& entry : fs::directory_iterator(local_storage_path)) {
            try {
                std::string ext = entry.path().extension().string();
                if (ext != ".ldb" && ext != ".log") continue;

                std::ifstream file(entry.path(), std::ios::binary);
                if (!file.is_open()) continue;

                std::string content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());

                LOG_INFO("scanning file: " + entry.path().string() +
                         " (" + std::to_string(content.size()) + " bytes)");

                // must have localConfig_v2 and at least one xoxc token
                bool has_config = content.find("ocalConfig_v2") != std::string::npos;
                bool has_xoxc = content.find("xoxc-") != std::string::npos;
                LOG_INFO("  config_v2=" + std::string(has_config ? "yes" : "no") +
                         " xoxc=" + std::string(has_xoxc ? "yes" : "no"));
                if (!has_config) continue;
                if (!has_xoxc) continue;

                LOG_INFO("found localConfig_v2 with tokens in " +
                         entry.path().filename().string());

                // brute-force Snappy decompression: try every byte offset
                // before the "ocalConfig_v2" marker as a potential Snappy
                // block start. the correct offset produces clean JSON.
                size_t config_pos = content.find("ocalConfig_v2");
                size_t scan_start = (config_pos > 65536) ? config_pos - 65536 : 0;

                std::string best;
                for (size_t try_off = scan_start; try_off < config_pos; try_off++) {
                    std::string dec = snappyDecompress(
                        reinterpret_cast<const uint8_t*>(content.data() + try_off),
                        std::min(content.size() - try_off, (size_t)200000));

                    if (dec.size() > 1000 &&
                        dec.find("xoxc-") != std::string::npos &&
                        dec.find("\"teams\"") != std::string::npos) {
                        if (dec.size() > best.size()) best = std::move(dec);
                    }
                }

                if (!best.empty()) {
                    LOG_INFO("decompressed Snappy block: " + std::to_string(best.size()) + " bytes");

                    size_t json_start = best.find("{\"teams\"");
                    if (json_start != std::string::npos) {
                        int depth = 0;
                        bool in_str = false, esc = false;
                        size_t json_end = json_start;
                        for (size_t i = json_start; i < best.size(); i++) {
                            char c = best[i];
                            if (esc) { esc = false; continue; }
                            if (c == '\\' && in_str) { esc = true; continue; }
                            if (c == '"') { in_str = !in_str; continue; }
                            if (in_str) continue;
                            if (c == '{') depth++;
                            if (c == '}') { depth--; if (depth == 0) { json_end = i + 1; break; } }
                        }

                        if (json_end > json_start) {
                            try {
                                auto config = nlohmann::json::parse(
                                    best.begin() + json_start, best.begin() + json_end);

                                if (config.contains("teams") && config["teams"].is_object()) {
                                    for (auto& [team_id, team_data] : config["teams"].items()) {
                                        if (!team_data.is_object()) continue;
                                        std::string token = team_data.value("token", "");
                                        if (token.find("xoxc-") != 0) continue;

                                        BrowserCredential cred;
                                        cred.browser_name = browser_name;
                                        cred.token = token;
                                        cred.team_id = team_id;
                                        cred.team_name = team_data.value("name", team_id);
                                        cred.team_url = team_data.value("url", "");
                                        cred.team_domain = team_data.value("domain", "");

                                        LOG_INFO("found team: " + cred.team_name +
                                                 " (" + cred.team_id + ") token_len=" +
                                                 std::to_string(cred.token.size()));
                                        results.push_back(std::move(cred));
                                    }
                                }
                            } catch (const std::exception& e) {
                                LOG_WARN("JSON parse error: " + std::string(e.what()));
                            }
                        }
                    }
                } else {
                    LOG_WARN("Snappy decompression failed for localConfig_v2 block");
                }

                if (!results.empty()) return results;

            } catch (...) {
                continue;
            }
        }
    } catch (...) {}

    return results;
}

std::optional<std::string> BrowserCredentials::extractToken(const std::string& local_storage_path) {
    // legacy fallback: scan for a bare xoxc- token when the v2 JSON parse fails.
    // this handles older single-workspace Slack installs.
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
                continue;
            }
        }
    } catch (...) {}

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

    // copy to temp so we don't fight Chrome for the file lock.
    // use CopyFileA with bFailIfExists=FALSE — this can read locked files
    // better than std::filesystem::copy_file on Windows.
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string tmp_path = std::string(tmp) + "conduit_cookies_" +
                            std::to_string(GetTickCount()) + ".db";

    // try Win32 CopyFile first (handles shared locks better)
    bool is_temp_copy = true;
    if (!CopyFileA(src_path.c_str(), tmp_path.c_str(), FALSE)) {
        // if CopyFile fails, try std::filesystem as fallback
        try {
            fs::copy_file(src_path, tmp_path, fs::copy_options::overwrite_existing);
        } catch (...) {
            // last resort: try reading the DB directly without copying
            LOG_WARN("couldn't copy cookie DB, trying direct read...");
            tmp_path = src_path;
            is_temp_copy = false;
        }
    }

    sqlite3* db = nullptr;
    // try opening with immutable flag — allows reading even when another
    // process has the file locked, since we promise not to write.
    // SQLite URIs need forward slashes and the path must be URI-encoded.
    std::string uri_path = tmp_path;
    for (auto& c : uri_path) { if (c == '\\') c = '/'; }
    std::string open_uri = "file:///" + uri_path + "?immutable=1";
    LOG_INFO("opening cookie DB: " + open_uri);
    int rc = sqlite3_open_v2(open_uri.c_str(), &db,
                              SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        LOG_WARN("immutable open failed, trying plain open...");
        if (db) { sqlite3_close(db); db = nullptr; }
        rc = sqlite3_open_v2(tmp_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    }
    if (rc != SQLITE_OK) {
        LOG_WARN("sqlite3_open failed for cookies: " + std::string(sqlite3_errmsg(db)));
        if (db) sqlite3_close(db);
        if (is_temp_copy) fs::remove(tmp_path);
        return std::nullopt;
    }

    std::optional<std::string> result;
    const char* sql = "SELECT encrypted_value FROM cookies "
                      "WHERE host_key = '.slack.com' AND name = 'd' "
                      "ORDER BY last_access_utc DESC LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    int prep_rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (prep_rc == SQLITE_OK) {
        int step_rc = sqlite3_step(stmt);
        if (step_rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blob_size = sqlite3_column_bytes(stmt, 0);
            LOG_INFO("cookie blob size: " + std::to_string(blob_size));
            if (blob && blob_size > 0) {
                std::vector<uint8_t> encrypted((uint8_t*)blob, (uint8_t*)blob + blob_size);
                // v20 = App-Bound Encryption (Chrome 127+ / Edge) — can't
                // decrypt without the browser's elevation service.
                // v10 = DPAPI-wrapped AES-GCM — we can decrypt this.
                std::string enc_ver(encrypted.begin(),
                    encrypted.begin() + std::min((size_t)3, encrypted.size()));
                if (enc_ver == "v20") {
                    LOG_WARN("cookie uses v20 App-Bound Encryption — "
                             "user must paste d= cookie manually");
                } else {
                    result = decryptChromeValue(encrypted, user_data_path);
                    if (result) {
                        LOG_INFO("decrypted Slack d= cookie (" +
                                 std::to_string(result->size()) + " chars)");
                    } else {
                        LOG_WARN("cookie decryption failed (ver=" + enc_ver + ")");
                    }
                }
            }
        } else {
            LOG_WARN("cookie query returned no rows (step_rc=" + std::to_string(step_rc) + ")");
        }
        sqlite3_finalize(stmt);
    } else {
        LOG_WARN("cookie query prepare failed: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_close(db);
    if (is_temp_copy) fs::remove(tmp_path);
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

                // the d cookie is per-browser (shared across all workspaces)
                auto cookie = extractCookie(path);
                std::string cookie_val = cookie.value_or("");

                for (auto& profile : profile_dirs) {
                    std::string ls_path = path +
#ifdef _WIN32
                        "\\" + profile + "\\Local Storage\\leveldb";
#else
                        "/" + profile + "/Local Storage/leveldb";
#endif

                    // try the v2 format (multi-workspace with team metadata)
                    auto teams = extractTeams(ls_path, name);
                    if (!teams.empty()) {
                        for (auto& team : teams) {
                            team.cookie = cookie_val;
                            // deduplicate across profiles
                            bool dup = false;
                            for (auto& r : results) {
                                if (r.token == team.token) { dup = true; break; }
                            }
                            if (!dup) results.push_back(std::move(team));
                        }
                        LOG_INFO("got " + std::to_string(results.size()) +
                                 " workspaces from " + name + " (" + profile + ")");
                        continue; // check other profiles too
                    }

                    // fallback: old single-token format
                    auto token = extractToken(ls_path);
                    if (!token) continue;

                    BrowserCredential cred;
                    cred.browser_name = name;
                    cred.token = *token;
                    cred.cookie = cookie_val;
                    cred.team_name = "Slack";
                    // deduplicate
                    bool dup = false;
                    for (auto& r : results) {
                        if (r.token == cred.token) { dup = true; break; }
                    }
                    if (!dup) results.push_back(std::move(cred));
                    LOG_INFO("got credentials from " + name + " (" + profile + ") [legacy]");
                }

                if (!results.empty()) break; // found what we need from this browser
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
