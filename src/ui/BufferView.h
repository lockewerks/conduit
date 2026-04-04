#pragma once
#include <string>
#include <vector>
#include "ui/Theme.h"

namespace conduit::ui {

// a fake message for the placeholder view
// the real one lives in slack/Types.h and is way more complicated
struct PlaceholderMsg {
    std::string time;
    std::string nick;
    std::string text;
    bool is_system = false;
};

class BufferView {
public:
    BufferView();
    void render(float x, float y, float width, float height, const Theme& theme);

private:
    std::vector<PlaceholderMsg> messages_;
    float scroll_y_ = 0.0f;
};

} // namespace conduit::ui
