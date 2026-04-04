#pragma once
#include <string>
#include <vector>
#include "ui/Theme.h"

namespace conduit::ui {

// left sidebar showing channels, DMs, etc
// right now it's all fake data but the structure is ready for real slack channels
struct BufferEntry {
    std::string name;
    bool is_active = false;
    bool has_unread = false;
    int unread_count = 0;
    bool is_dm = false;
    bool is_separator = false; // org header
};

class BufferList {
public:
    BufferList();
    void render(float x, float y, float width, float height, const Theme& theme);

    int selectedIndex() const { return selected_; }
    void select(int index);
    void selectNext();
    void selectPrev();

    const std::vector<BufferEntry>& entries() const { return entries_; }

private:
    std::vector<BufferEntry> entries_;
    int selected_ = 0;
    float scroll_y_ = 0.0f;
};

} // namespace conduit::ui
