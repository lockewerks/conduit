#pragma once
#include <string>
#include <vector>
#include <functional>

namespace conduit::input {

// tab completion for @users, #channels, :emoji:, /commands
// cycles through matches on repeated Tab presses
class TabComplete {
public:
    using CandidateProvider = std::function<std::vector<std::string>(const std::string& prefix)>;

    void addProvider(char trigger, CandidateProvider provider);

    // given the current input and cursor position, get the completion
    // returns the completed text, or empty string if nothing to complete
    struct CompletionResult {
        std::string completed_text;    // the full input with completion applied
        int new_cursor_pos = 0;
        bool has_candidates = false;
    };

    CompletionResult complete(const std::string& input, int cursor_pos);
    CompletionResult completeReverse(const std::string& input, int cursor_pos);

    // reset the cycling state (call when input changes)
    void reset();

private:
    struct ProviderEntry {
        char trigger;
        CandidateProvider provider;
    };

    std::vector<ProviderEntry> providers_;
    std::vector<std::string> current_candidates_;
    int cycle_index_ = -1;
    std::string last_prefix_;
    int last_trigger_pos_ = -1;
};

} // namespace conduit::input
