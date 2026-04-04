#include "util/TimeFormat.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <chrono>

namespace conduit::util {

time_t parseSlackTs(const std::string& ts) {
    if (ts.empty()) return 0;

    // slack timestamps: "1234567890.123456"
    // we only care about the integer part (seconds since epoch)
    // the fractional part is slack's internal ordering, not real time precision
    auto dot = ts.find('.');
    std::string seconds_str = (dot != std::string::npos) ? ts.substr(0, dot) : ts;

    try {
        return static_cast<time_t>(std::stoll(seconds_str));
    } catch (...) {
        return 0; // garbage in, zero out
    }
}

std::string formatTime(const std::string& ts) {
    time_t t = parseSlackTs(ts);
    if (t == 0) return "??:??";

    struct tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
    return buf;
}

std::string formatRelative(const std::string& ts) {
    time_t t = parseSlackTs(ts);
    if (t == 0) return "???";

    time_t now = std::time(nullptr);
    double diff = std::difftime(now, t);

    if (diff < 0) return "from the future"; // clock skew is fun

    if (diff < 60) return "just now";
    if (diff < 3600) {
        int mins = static_cast<int>(diff / 60);
        return std::to_string(mins) + "m ago";
    }
    if (diff < 86400) {
        int hours = static_cast<int>(diff / 3600);
        return std::to_string(hours) + "h ago";
    }
    if (diff < 172800) return "yesterday";

    int days = static_cast<int>(diff / 86400);
    if (days < 30) return std::to_string(days) + "d ago";

    // at this point just show the date, nobody cares about "47d ago"
    return formatDate(ts);
}

std::string formatFull(const std::string& ts) {
    time_t t = parseSlackTs(ts);
    if (t == 0) return "Unknown time";

    struct tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    // abbreviated day and month names because strftime is locale-dependent and we don't trust it
    static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    int hour12 = local_tm.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char* ampm = local_tm.tm_hour >= 12 ? "PM" : "AM";

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s %s %d, %d at %d:%02d %s",
                  days[local_tm.tm_wday], months[local_tm.tm_mon],
                  local_tm.tm_mday, local_tm.tm_year + 1900,
                  hour12, local_tm.tm_min, ampm);
    return buf;
}

std::string formatDate(const std::string& ts) {
    time_t t = parseSlackTs(ts);
    if (t == 0) return "Unknown date";

    struct tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s %d, %d",
                  months[local_tm.tm_mon], local_tm.tm_mday, local_tm.tm_year + 1900);
    return buf;
}

bool isSameDay(const std::string& ts1, const std::string& ts2) {
    time_t t1 = parseSlackTs(ts1);
    time_t t2 = parseSlackTs(ts2);
    if (t1 == 0 || t2 == 0) return false;

    struct tm tm1, tm2;
#ifdef _WIN32
    localtime_s(&tm1, &t1);
    localtime_s(&tm2, &t2);
#else
    localtime_r(&t1, &tm1);
    localtime_r(&t2, &tm2);
#endif

    return tm1.tm_year == tm2.tm_year &&
           tm1.tm_yday == tm2.tm_yday;
}

} // namespace conduit::util
