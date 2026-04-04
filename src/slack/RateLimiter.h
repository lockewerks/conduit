#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <mutex>

namespace conduit::slack {

// slack rate limiting tiers, because apparently "don't DDoS us" needs a taxonomy
enum class RateTier { Tier1, Tier2, Tier3, Tier4, Special };

class RateLimiter {
public:
    // record that we just made a call to this method
    void recordCall(const std::string& method);

    // check if we should hold off on calling this method
    bool shouldThrottle(const std::string& method) const;

    // if we got a 429, record the retry-after time
    void setRetryAfter(const std::string& method, int seconds);

    // how many seconds until we can call this method again (0 = go ahead)
    int retryAfter(const std::string& method) const;

    // figure out the rate tier for a given slack API method
    static RateTier tierForMethod(const std::string& method);

private:
    struct MethodState {
        std::deque<std::chrono::steady_clock::time_point> call_times;
        std::chrono::steady_clock::time_point blocked_until;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MethodState> states_;

    // max calls per minute per tier
    static int maxCallsPerMinute(RateTier tier);
};

} // namespace conduit::slack
