#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace conduit::slack {

// wrapper around libcurl for slack's REST API
// handles auth headers, json parsing, error handling, all that fun stuff
class WebAPI {
public:
    explicit WebAPI(const std::string& token);
    ~WebAPI();

    // generic GET/POST to slack API
    // returns parsed json or nullopt on failure
    std::optional<nlohmann::json> get(const std::string& method,
                                       const std::string& params = "");
    std::optional<nlohmann::json> post(const std::string& method,
                                        const nlohmann::json& body);
    std::optional<nlohmann::json> postForm(const std::string& method,
                                            const std::string& params);

    // download a file (for images, etc). needs auth header for url_private
    std::vector<uint8_t> downloadFile(const std::string& url);

    // upload a file using multipart form data
    std::optional<nlohmann::json> uploadFile(const std::string& channel,
                                              const std::string& filepath,
                                              const std::string& title = "");

    void setToken(const std::string& token) { token_ = token; }
    void setTimeout(int seconds) { timeout_ = seconds; }

    // last HTTP status code for debugging
    long lastStatus() const { return last_status_; }
    // last retry-after header value (0 if none)
    int lastRetryAfter() const { return last_retry_after_; }

private:
    std::string token_;
    int timeout_ = 30;
    long last_status_ = 0;
    int last_retry_after_ = 0;

    // curl callback
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userp);

    CURL* createHandle(const std::string& url);
    std::optional<nlohmann::json> performAndParse(CURL* curl);
};

} // namespace conduit::slack
