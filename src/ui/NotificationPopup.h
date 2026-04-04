#pragma once
#include "ui/Theme.h"
#include <string>
#include <chrono>

namespace conduit::ui {

// little toast that pops up in the corner for a few seconds
// like a real desktop notification but uglier and inside the app
class NotificationPopup {
public:
    void show(const std::string& title, const std::string& body);
    void render(float screen_w, float screen_h, const Theme& theme);

    // how long the popup sticks around (default 4s, enough to read but not annoying)
    void setDuration(float seconds) { duration_ = seconds; }

private:
    std::string title_;
    std::string body_;
    bool visible_ = false;
    float duration_ = 4.0f;
    std::chrono::steady_clock::time_point show_time_;

    // opacity ramp for fade-in/fade-out because we have standards
    float currentAlpha() const;
};

} // namespace conduit::ui
