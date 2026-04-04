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

private:
    std::vector<NickEntry> nicks_;
};

} // namespace conduit::ui
