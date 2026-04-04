#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <vector>
#include <cstdint>

namespace conduit::cache {

// disk-backed file cache with LRU eviction
// stores downloaded images, avatars, and whatever else slack throws at us
// so we don't re-download the same profile pic 400 times per session
class FileCache {
public:
    // default cap of 500MB, because disk space is cheap but not infinite
    explicit FileCache(const std::string& cache_dir, uint64_t max_bytes = 500ULL * 1024 * 1024);

    // store data on disk, returns the local file path
    std::string store(const std::string& url, const std::vector<uint8_t>& data);

    // check if we have a cached copy, returns the path if so
    std::optional<std::string> get(const std::string& url);

    // evict oldest files until we're under the size limit
    void prune();

    // how much space the cache is currently hogging
    uint64_t currentSize() const;

    // blow the whole cache away (for when the user wants a fresh start)
    void clear();

private:
    std::string cache_dir_;
    uint64_t max_bytes_;
    mutable std::mutex mutex_;

    // turn a URL into a safe filename - hashing because URLs are disgusting paths
    std::string urlToFilename(const std::string& url) const;

    // get the full path for a cached file
    std::string pathFor(const std::string& filename) const;

    // touch a file's modification time so LRU knows it was recently used
    void touchFile(const std::string& path) const;
};

} // namespace conduit::cache
