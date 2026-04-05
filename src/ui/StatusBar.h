#pragma once
#include <string>
#include <vector>
#include "ui/Theme.h"

namespace conduit::ui {

class StatusBar {
public:
    void render(float x, float y, float width, float height, const Theme& theme);

    void setOrgName(const std::string& name) { org_name_ = name; }
    void setConnectionState(const std::string& state) { connection_state_ = state; }
    void setUnreadCount(int count) { unread_count_ = count; }
    void setTypingUsers(const std::vector<std::string>& users) { typing_users_ = users; }

private:
    std::string org_name_ = "Conduit";
    std::string connection_state_ = "not connected";
    int unread_count_ = 0;
    std::vector<std::string> typing_users_;
};

} // namespace conduit::ui
