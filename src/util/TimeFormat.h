#pragma once
#include <string>
#include <ctime>

namespace conduit::util {

// timestamp formatting for slack's weird "seconds.microseconds" string format
// because slack couldn't just use ISO 8601 like normal people

// parse slack's timestamp string into a time_t
// slack timestamps look like "1234567890.123456" - the part before the dot is unix epoch
time_t parseSlackTs(const std::string& ts);

// "HH:MM" - for message timestamps in the buffer view
std::string formatTime(const std::string& ts);

// "2m ago", "3h ago", "yesterday" - for relative display
std::string formatRelative(const std::string& ts);

// "Mon Apr 4, 2026 at 3:42 PM" - for tooltips and full date display
std::string formatFull(const std::string& ts);

// "Apr 4, 2026" - date only, for date separators between messages
std::string formatDate(const std::string& ts);

// check if two timestamps are on the same calendar day
bool isSameDay(const std::string& ts1, const std::string& ts2);

} // namespace conduit::util
