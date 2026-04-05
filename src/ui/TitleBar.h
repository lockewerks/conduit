#pragma once
#include <string>
#include "ui/Theme.h"

namespace conduit::ui {

class TitleBar {
public:
    void render(float x, float y, float width, float height, const Theme& theme);

    void setChannelName(const std::string& name) { channel_name_ = name; }
    void setTopic(const std::string& topic) { topic_ = topic; }
    void setMemberCount(int count) { member_count_ = count; }

private:
    std::string channel_name_ = "#general";
    std::string topic_ = "Welcome to Conduit. You look like you need coffee.";
    int member_count_ = 0;
};

} // namespace conduit::ui
