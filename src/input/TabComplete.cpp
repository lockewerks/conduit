#include "input/TabComplete.h"
#include <algorithm>

namespace conduit::input {

void TabComplete::addProvider(char trigger, CandidateProvider provider) {
    providers_.push_back({trigger, std::move(provider)});
}

TabComplete::CompletionResult TabComplete::complete(const std::string& input, int cursor_pos) {
    CompletionResult result;
    result.completed_text = input;
    result.new_cursor_pos = cursor_pos;

    if (input.empty() || cursor_pos <= 0) return result;

    // if we're cycling through existing candidates, just advance
    if (!current_candidates_.empty() && cycle_index_ >= 0) {
        cycle_index_ = (cycle_index_ + 1) % (int)current_candidates_.size();
        std::string candidate = current_candidates_[cycle_index_];
        std::string before = input.substr(0, last_trigger_pos_);
        std::string after = (cursor_pos < (int)input.size()) ? input.substr(cursor_pos) : "";
        result.completed_text = before + candidate + " " + after;
        result.new_cursor_pos = (int)(before.size() + candidate.size() + 1);
        result.has_candidates = true;
        return result;
    }

    // find the word being typed (scan backward from cursor)
    int word_start = cursor_pos - 1;
    while (word_start > 0 && input[word_start - 1] != ' ') {
        word_start--;
    }

    std::string word = input.substr(word_start, cursor_pos - word_start);
    if (word.empty()) return result;

    // find a matching provider based on the first character
    char trigger = word[0];
    std::string prefix = word;

    // special case: / at the start of input completes commands
    if (trigger == '/' && word_start == 0) {
        prefix = word.substr(1); // strip the /
    }

    for (auto& provider : providers_) {
        if (provider.trigger == trigger) {
            auto candidates = provider.provider(prefix.length() > 1 ? prefix.substr(1) : "");
            if (candidates.empty()) continue;

            // filter by prefix
            std::string match_prefix = (prefix.length() > 1) ? prefix.substr(1) : "";
            std::vector<std::string> filtered;
            for (auto& c : candidates) {
                if (match_prefix.empty() ||
                    c.substr(0, match_prefix.size()) == match_prefix) {
                    // re-add the trigger
                    filtered.push_back(std::string(1, trigger) + c);
                }
            }

            if (filtered.empty()) continue;

            std::sort(filtered.begin(), filtered.end());
            current_candidates_ = filtered;
            cycle_index_ = 0;
            last_prefix_ = prefix;
            last_trigger_pos_ = word_start;

            std::string candidate = current_candidates_[0];
            std::string before = input.substr(0, word_start);
            std::string after = (cursor_pos < (int)input.size()) ? input.substr(cursor_pos) : "";
            result.completed_text = before + candidate + " " + after;
            result.new_cursor_pos = (int)(before.size() + candidate.size() + 1);
            result.has_candidates = true;
            return result;
        }
    }

    return result;
}

TabComplete::CompletionResult TabComplete::completeReverse(const std::string& input,
                                                            int cursor_pos) {
    CompletionResult result;
    result.completed_text = input;
    result.new_cursor_pos = cursor_pos;

    if (current_candidates_.empty() || cycle_index_ < 0) return result;

    cycle_index_--;
    if (cycle_index_ < 0) cycle_index_ = (int)current_candidates_.size() - 1;

    std::string candidate = current_candidates_[cycle_index_];
    std::string before = input.substr(0, last_trigger_pos_);
    std::string after = (cursor_pos < (int)input.size()) ? input.substr(cursor_pos) : "";
    result.completed_text = before + candidate + " " + after;
    result.new_cursor_pos = (int)(before.size() + candidate.size() + 1);
    result.has_candidates = true;
    return result;
}

void TabComplete::reset() {
    current_candidates_.clear();
    cycle_index_ = -1;
    last_prefix_.clear();
    last_trigger_pos_ = -1;
}

} // namespace conduit::input
