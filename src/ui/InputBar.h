#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <functional>
#include "ui/Theme.h"
#include "input/InputHistory.h"
#include "input/TabComplete.h"

namespace conduit::ui {

// autocomplete provider: given a trigger char and prefix, return matches
using AutocompleteProvider = std::function<std::vector<std::string>(char trigger, const std::string& prefix)>;

class InputBar {
public:
    float render(float x, float y, float width, float max_height, const Theme& theme);

    const std::string& getText() const { return text_; }
    void clear() { text_.clear(); std::memset(input_buf_, 0, sizeof(input_buf_)); }
    bool submitted() const { return submitted_; }
    void setChannelName(const std::string& name) { channel_name_ = name; }
    void setHistory(input::InputHistory* h) { history_ = h; }
    void setChannelId(const std::string& id) { channel_id_ = id; }
    void pasteText(const std::string& text);

    void setAutocompleteProvider(AutocompleteProvider p) { ac_provider_ = std::move(p); }
    void setTabComplete(input::TabComplete* tc) { tab_complete_ = tc; }

    void setText(const std::string& text) {
        text_ = text;
        std::memset(input_buf_, 0, sizeof(input_buf_));
        std::memcpy(input_buf_, text.data(), std::min(text.size(), sizeof(input_buf_) - 1));
        focus_input_ = true;
    }

private:
    std::string text_;
    std::string channel_name_ = "#general";
    char input_buf_[4096] = {};
    bool submitted_ = false;
    bool focus_input_ = true;
    input::InputHistory* history_ = nullptr;
    input::TabComplete* tab_complete_ = nullptr;
    std::string channel_id_;

    // tab complete tracking
    std::string last_input_for_tab_;

    // autocomplete state
    AutocompleteProvider ac_provider_;
    std::vector<std::string> ac_matches_;
    int ac_selected_ = 0;
    bool ac_open_ = false;
    char ac_trigger_ = 0;
    int ac_trigger_pos_ = -1;    // position of trigger char in input_buf_
    std::string ac_prefix_;

    void updateAutocomplete();
    void applyAutocomplete(const std::string& match);
};

} // namespace conduit::ui
