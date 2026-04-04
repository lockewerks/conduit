#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace conduit::input {

// per-channel input history so you can up-arrow through your past messages
// just like a real terminal, except your messages went to slack and you can't take them back
class InputHistory {
public:
    void add(const std::string& channel_id, const std::string& text);
    std::string prev(const std::string& channel_id);
    std::string next(const std::string& channel_id);
    void resetPosition(const std::string& channel_id);

    int maxEntries() const { return max_entries_; }
    void setMaxEntries(int max) { max_entries_ = max; }

private:
    struct ChannelHistory {
        std::vector<std::string> entries;
        int position = -1; // -1 = not browsing, 0 = most recent
    };

    std::unordered_map<std::string, ChannelHistory> history_;
    int max_entries_ = 100;
};

} // namespace conduit::input
