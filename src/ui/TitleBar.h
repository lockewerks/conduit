#pragma once
#include <string>
#include "ui/Theme.h"
#include <SDL.h>

namespace conduit::ui {

// custom window title bar - replaces the OS chrome
// handles drag-to-move, double-click-to-maximize, and min/max/close buttons
class TitleBar {
public:
    void render(float x, float y, float width, float height, const Theme& theme);

    void setChannelName(const std::string& name) { channel_name_ = name; }
    void setTopic(const std::string& topic) { topic_ = topic; }
    void setMemberCount(int count) { member_count_ = count; }
    void setWindow(SDL_Window* w) { window_ = w; }

    // did the user click a window control this frame?
    bool wantsClose() const { return wants_close_; }
    bool wantsMinimize() const { return wants_minimize_; }
    bool wantsMaximize() const { return wants_maximize_; }

private:
    std::string channel_name_ = "#general";
    std::string topic_ = "Welcome to Conduit.";
    int member_count_ = 0;
    SDL_Window* window_ = nullptr;

    bool wants_close_ = false;
    bool wants_minimize_ = false;
    bool wants_maximize_ = false;
    bool dragging_ = false;
    int drag_start_x_ = 0;
    int drag_start_y_ = 0;
};

} // namespace conduit::ui
