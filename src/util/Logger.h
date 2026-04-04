#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace conduit {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

// a logger that writes to file and also keeps recent entries in memory
// so we can show them in a /debug buffer later
class Logger {
public:
    static Logger& instance() {
        static Logger s;
        return s;
    }

    void setLevel(LogLevel level) { level_ = level; }
    void setFile(const std::string& path);

    void log(LogLevel level, const std::string& msg);
    void trace(const std::string& msg) { log(LogLevel::Trace, msg); }
    void debug(const std::string& msg) { log(LogLevel::Debug, msg); }
    void info(const std::string& msg) { log(LogLevel::Info, msg); }
    void warn(const std::string& msg) { log(LogLevel::Warn, msg); }
    void error(const std::string& msg) { log(LogLevel::Error, msg); }

    // grab recent log lines for the debug buffer
    std::vector<std::string> getRecent(int count = 100) const;

private:
    Logger() = default;
    ~Logger();

    LogLevel level_ = LogLevel::Info;
    std::ofstream file_;
    mutable std::mutex mutex_;

    // ring buffer of recent log entries, because we're not allocating a deque for this
    static constexpr int kMaxEntries = 500;
    std::vector<std::string> entries_;

    static const char* levelStr(LogLevel l);
    std::string timestamp() const;
};

// convenience macros because typing Logger::instance().info() gets old fast
#define LOG_TRACE(msg) conduit::Logger::instance().trace(msg)
#define LOG_DEBUG(msg) conduit::Logger::instance().debug(msg)
#define LOG_INFO(msg) conduit::Logger::instance().info(msg)
#define LOG_WARN(msg) conduit::Logger::instance().warn(msg)
#define LOG_ERROR(msg) conduit::Logger::instance().error(msg)

} // namespace conduit
