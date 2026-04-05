#pragma once
#include <string>
#include <cstring>
#include "ui/Theme.h"
#include "input/InputHistory.h"

namespace conduit::ui {

class InputBar {
public:
    // returns the height it actually needs (for dynamic sizing)
    float render(float x, float y, float width, float max_height, const Theme& theme);

    const std::string& getText() const { return text_; }
    void clear() { text_.clear(); std::memset(input_buf_, 0, sizeof(input_buf_)); }
    bool submitted() const { return submitted_; }
    void setChannelName(const std::string& name) { channel_name_ = name; }
    void setHistory(input::InputHistory* h) { history_ = h; }
    void setChannelId(const std::string& id) { channel_id_ = id; }
    void pasteText(const std::string& text);

private:
    std::string text_;
    std::string channel_name_ = "#general";
    char input_buf_[4096] = {};
    bool submitted_ = false;
    bool focus_input_ = true;
    input::InputHistory* history_ = nullptr;
    std::string channel_id_;
};

} // namespace conduit::ui
