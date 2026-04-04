#include "input/InputHistory.h"

namespace conduit::input {

void InputHistory::add(const std::string& channel_id, const std::string& text) {
    if (text.empty()) return;

    auto& h = history_[channel_id];

    // don't add duplicates of the last entry
    if (!h.entries.empty() && h.entries.back() == text) return;

    h.entries.push_back(text);
    if ((int)h.entries.size() > max_entries_) {
        h.entries.erase(h.entries.begin());
    }
    h.position = -1;
}

std::string InputHistory::prev(const std::string& channel_id) {
    auto& h = history_[channel_id];
    if (h.entries.empty()) return "";

    if (h.position == -1) {
        h.position = (int)h.entries.size() - 1;
    } else if (h.position > 0) {
        h.position--;
    }

    return h.entries[h.position];
}

std::string InputHistory::next(const std::string& channel_id) {
    auto& h = history_[channel_id];
    if (h.position == -1) return "";

    h.position++;
    if (h.position >= (int)h.entries.size()) {
        h.position = -1;
        return "";
    }

    return h.entries[h.position];
}

void InputHistory::resetPosition(const std::string& channel_id) {
    auto it = history_.find(channel_id);
    if (it != history_.end()) {
        it->second.position = -1;
    }
}

} // namespace conduit::input
