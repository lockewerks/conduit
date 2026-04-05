#pragma once
#include <string>
#include <vector>
#include "ui/Theme.h"

namespace conduit::ui {

struct NickEntry {
    std::string name;
    bool is_online = true;
    bool is_bot = false;
};

class NickList {
public:
    NickList();
    void render(float x, float y, float width, float height, const Theme& theme);
    void setNicks(const std::vector<NickEntry>& nicks) { nicks_ = nicks; }

    // right-click on a nick - parent can check and open a DM or whatever
    const std::string& lastClickedNick() const { return clicked_nick_; }
    void clearClickedNick() { clicked_nick_.clear(); }

private:
    std::vector<NickEntry> nicks_;
    std::string clicked_nick_;
};

} // namespace conduit::ui
