#pragma once
#include <string>
#include "ui/Theme.h"

namespace conduit::ui {

class InputBar {
public:
    void render(float x, float y, float width, float height, const Theme& theme);

    const std::string& getText() const { return text_; }
    void clear() { text_.clear(); }

    // returns true if the user just hit enter
    bool submitted() const { return submitted_; }

private:
    std::string text_;
    char input_buf_[4096] = {};
    bool submitted_ = false;
    bool focus_input_ = true;
};

} // namespace conduit::ui
