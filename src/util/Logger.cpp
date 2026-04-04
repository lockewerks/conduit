#include "util/Logger.h"
#include <iostream>

namespace conduit {

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.close();
    file_.open(path, std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "couldn't open log file: " << path << "\n";
    }
}

Logger::~Logger() {
    if (file_.is_open()) file_.close();
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < level_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::string line = timestamp() + " [" + levelStr(level) + "] " + msg;

    // keep in memory for the debug buffer
    if (entries_.size() >= kMaxEntries) {
        entries_.erase(entries_.begin());
    }
    entries_.push_back(line);

    // write to file if we have one
    if (file_.is_open()) {
        file_ << line << "\n";
        file_.flush(); // yeah yeah, performance. but missing logs on crash is worse.
    }

    // also stderr for debug builds because printf debugging never dies
#ifndef NDEBUG
    std::cerr << line << "\n";
#endif
}

std::vector<std::string> Logger::getRecent(int count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int start = static_cast<int>(entries_.size()) - count;
    if (start < 0) start = 0;
    return std::vector<std::string>(entries_.begin() + start, entries_.end());
}

const char* Logger::levelStr(LogLevel l) {
    switch (l) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO ";
    case LogLevel::Warn: return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?????"; // how did you even get here
}

std::string Logger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream ss;
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    ss << std::put_time(&tm_buf, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3)
       << ms.count();
    return ss.str();
}

} // namespace conduit
