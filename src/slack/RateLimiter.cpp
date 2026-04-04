#include "slack/RateLimiter.h"
#include "util/Logger.h"

namespace conduit::slack {

using Clock = std::chrono::steady_clock;

void RateLimiter::recordCall(const std::string& method) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = states_[method];
    state.call_times.push_back(Clock::now());

    // prune entries older than 60 seconds
    auto cutoff = Clock::now() - std::chrono::seconds(60);
    while (!state.call_times.empty() && state.call_times.front() < cutoff) {
        state.call_times.pop_front();
    }
}

bool RateLimiter::shouldThrottle(const std::string& method) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = states_.find(method);
    if (it == states_.end()) return false;

    auto& state = it->second;

    // check if we're in a retry-after window
    if (Clock::now() < state.blocked_until) return true;

    // check if we're approaching the rate limit
    RateTier tier = tierForMethod(method);
    int max_rpm = maxCallsPerMinute(tier);

    // count calls in the last 60 seconds
    auto cutoff = Clock::now() - std::chrono::seconds(60);
    int recent = 0;
    for (auto& t : state.call_times) {
        if (t >= cutoff) recent++;
    }

    // leave some headroom (80% of limit)
    return recent >= (max_rpm * 80 / 100);
}

void RateLimiter::setRetryAfter(const std::string& method, int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    states_[method].blocked_until = Clock::now() + std::chrono::seconds(seconds);
    LOG_WARN("rate limited on " + method + ", backing off for " + std::to_string(seconds) + "s");
}

int RateLimiter::retryAfter(const std::string& method) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = states_.find(method);
    if (it == states_.end()) return 0;

    auto remaining = it->second.blocked_until - Clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(remaining).count();
    return secs > 0 ? static_cast<int>(secs) : 0;
}

RateTier RateLimiter::tierForMethod(const std::string& method) {
    // tier 1: write operations (~1/sec)
    if (method == "chat.postMessage" || method == "chat.update" ||
        method == "chat.delete" || method == "reactions.add" ||
        method == "reactions.remove" || method == "pins.add" ||
        method == "pins.remove") {
        return RateTier::Tier1;
    }

    // tier 2: read with pagination (~20/min)
    if (method == "conversations.history" || method == "conversations.replies" ||
        method == "conversations.members") {
        return RateTier::Tier2;
    }

    // tier 3: list operations (~50/min)
    if (method == "conversations.list" || method == "users.list" ||
        method == "search.messages") {
        return RateTier::Tier3;
    }

    // special: file uploads are slow
    if (method == "files.upload") {
        return RateTier::Special;
    }

    // tier 4: everything else (info queries, auth.test, etc)
    return RateTier::Tier4;
}

int RateLimiter::maxCallsPerMinute(RateTier tier) {
    switch (tier) {
    case RateTier::Tier1: return 60;     // ~1/sec
    case RateTier::Tier2: return 20;
    case RateTier::Tier3: return 50;
    case RateTier::Tier4: return 100;
    case RateTier::Special: return 20;   // files.upload
    }
    return 60;
}

} // namespace conduit::slack
