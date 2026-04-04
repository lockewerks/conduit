#include "slack/WebAPI.h"
#include "util/Logger.h"
#include <sstream>
#include <fstream>

namespace conduit::slack {

static const std::string SLACK_API_BASE = "https://slack.com/api/";

WebAPI::WebAPI(const std::string& token) : token_(token) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

WebAPI::~WebAPI() {
    // we don't call curl_global_cleanup here because other instances might exist
    // it's fine. probably. the OS will clean up after us anyway.
}

size_t WebAPI::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

size_t WebAPI::headerCallback(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t total = size * nitems;
    auto* api = static_cast<WebAPI*>(userp);

    std::string header(buffer, total);
    // look for Retry-After header (slack sends this with 429s)
    if (header.find("retry-after:") != std::string::npos ||
        header.find("Retry-After:") != std::string::npos) {
        auto pos = header.find(':');
        if (pos != std::string::npos) {
            try {
                api->last_retry_after_ = std::stoi(header.substr(pos + 1));
            } catch (...) {
                api->last_retry_after_ = 5; // safe default
            }
        }
    }
    return total;
}

CURL* WebAPI::createHandle(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // auth header
    struct curl_slist* headers = nullptr;
    std::string auth = "Authorization: Bearer " + token_;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    return curl;
}

std::optional<nlohmann::json> WebAPI::performAndParse(CURL* curl) {
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    last_retry_after_ = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &last_status_);

    // clean up the header list
    struct curl_slist* headers = nullptr;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR(std::string("curl error: ") + curl_easy_strerror(res));
        return std::nullopt;
    }

    if (last_status_ == 429) {
        LOG_WARN("rate limited! retry after " + std::to_string(last_retry_after_) + "s");
        return std::nullopt;
    }

    try {
        auto j = nlohmann::json::parse(response);
        if (!j.value("ok", false)) {
            std::string error = j.value("error", "unknown");
            LOG_WARN("slack API error: " + error);
        }
        return j;
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR(std::string("json parse error: ") + e.what());
        return std::nullopt;
    }
}

std::optional<nlohmann::json> WebAPI::get(const std::string& method,
                                           const std::string& params) {
    std::string url = SLACK_API_BASE + method;
    if (!params.empty()) url += "?" + params;

    CURL* curl = createHandle(url);
    if (!curl) return std::nullopt;

    LOG_DEBUG("GET " + method);
    return performAndParse(curl);
}

std::optional<nlohmann::json> WebAPI::post(const std::string& method,
                                            const nlohmann::json& body) {
    std::string url = SLACK_API_BASE + method;
    CURL* curl = createHandle(url);
    if (!curl) return std::nullopt;

    std::string body_str = body.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body_str.size());

    LOG_DEBUG("POST " + method);
    return performAndParse(curl);
}

std::optional<nlohmann::json> WebAPI::postForm(const std::string& method,
                                                const std::string& params) {
    std::string url = SLACK_API_BASE + method;
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());

    struct curl_slist* headers = nullptr;
    std::string auth = "Authorization: Bearer " + token_;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    LOG_DEBUG("POST (form) " + method);
    auto result = performAndParse(curl);
    curl_slist_free_all(headers);
    return result;
}

std::vector<uint8_t> WebAPI::downloadFile(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60); // files can be big
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // slack's url_private needs auth
    struct curl_slist* headers = nullptr;
    std::string auth = "Authorization: Bearer " + token_;
    headers = curl_slist_append(headers, auth.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        LOG_ERROR("file download failed: " + url);
        return {};
    }

    return std::vector<uint8_t>(data.begin(), data.end());
}

std::optional<nlohmann::json> WebAPI::uploadFile(const std::string& channel,
                                                  const std::string& filepath,
                                                  const std::string& title) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    std::string url = SLACK_API_BASE + "files.upload";

    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* part;

    // the file itself
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filedata(part, filepath.c_str());

    // channel
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "channels");
    curl_mime_data(part, channel.c_str(), CURL_ZERO_TERMINATED);

    // title
    if (!title.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "title");
        curl_mime_data(part, title.c_str(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120); // uploads can take a while

    struct curl_slist* headers = nullptr;
    std::string auth = "Authorization: Bearer " + token_;
    headers = curl_slist_append(headers, auth.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        LOG_ERROR("file upload failed");
        return std::nullopt;
    }

    try {
        return nlohmann::json::parse(response);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace conduit::slack
