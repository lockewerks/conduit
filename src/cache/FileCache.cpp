#include "cache/FileCache.h"
#include "util/Platform.h"
#include "util/Logger.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>

namespace fs = std::filesystem;

namespace conduit::cache {

FileCache::FileCache(const std::string& cache_dir, uint64_t max_bytes)
    : cache_dir_(cache_dir), max_bytes_(max_bytes) {
    platform::ensureDir(cache_dir_);
}

std::string FileCache::store(const std::string& url, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string filename = urlToFilename(url);
    std::string path = pathFor(filename);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        LOG_ERROR("FileCache: couldn't write to " + path + " - disk full? permissions?");
        return "";
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();

    LOG_DEBUG("FileCache: stored " + std::to_string(data.size()) + " bytes for " + url);
    return path;
}

std::optional<std::string> FileCache::get(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string filename = urlToFilename(url);
    std::string path = pathFor(filename);

    if (!fs::exists(path)) return std::nullopt;

    // bump the mod time so prune() knows this file is still wanted
    touchFile(path);
    return path;
}

void FileCache::prune() {
    std::lock_guard<std::mutex> lock(mutex_);

    // collect all files with their sizes and mod times
    struct CacheEntry {
        fs::path path;
        uint64_t size;
        fs::file_time_type mtime;
    };

    std::vector<CacheEntry> entries;
    uint64_t total = 0;

    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cache_dir_, ec)) {
        if (!entry.is_regular_file()) continue;
        uint64_t sz = entry.file_size();
        entries.push_back({entry.path(), sz, entry.last_write_time()});
        total += sz;
    }

    if (total <= max_bytes_) return; // we're fine, chill

    // sort by mod time, oldest first - those get the axe
    std::sort(entries.begin(), entries.end(),
              [](const CacheEntry& a, const CacheEntry& b) {
                  return a.mtime < b.mtime;
              });

    int evicted = 0;
    for (auto& e : entries) {
        if (total <= max_bytes_) break;
        fs::remove(e.path, ec);
        if (!ec) {
            total -= e.size;
            evicted++;
        }
    }

    LOG_INFO("FileCache: pruned " + std::to_string(evicted) + " files, "
             + std::to_string(total / (1024 * 1024)) + "MB remaining");
}

uint64_t FileCache::currentSize() const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t total = 0;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cache_dir_, ec)) {
        if (entry.is_regular_file()) {
            total += entry.file_size();
        }
    }
    return total;
}

void FileCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cache_dir_, ec)) {
        fs::remove(entry.path(), ec);
    }
    LOG_INFO("FileCache: nuked from orbit");
}

std::string FileCache::urlToFilename(const std::string& url) const {
    // std::hash is not cryptographic but we don't need it to be - just unique-ish
    size_t hash = std::hash<std::string>{}(url);

    // try to preserve the file extension so we know what we're dealing with
    std::string ext;
    auto dot = url.rfind('.');
    auto slash = url.rfind('/');
    auto question = url.rfind('?');

    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        size_t end = (question != std::string::npos && question > dot) ? question : url.size();
        ext = url.substr(dot, std::min(end - dot, (size_t)10)); // cap extension length, some URLs are insane
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%016zx%s", hash, ext.c_str());
    return std::string(buf);
}

std::string FileCache::pathFor(const std::string& filename) const {
    return cache_dir_ + "/" + filename;
}

void FileCache::touchFile(const std::string& path) const {
    // update the modification time to now
    std::error_code ec;
    fs::last_write_time(path, fs::file_time_type::clock::now(), ec);
    // if this fails, whatever, LRU will be slightly off
}

} // namespace conduit::cache
