#pragma once
#include <string>
#include <vector>
#include "ui/Theme.h"

namespace conduit::ui {

struct BufferEntry {
    std::string name;
    bool is_active = false;
    bool has_unread = false;
    int unread_count = 0;
    bool is_dm = false;
    bool is_separator = false;
    std::string channel_id; // the slack channel ID for switching
};

class BufferList {
public:
    BufferList();
    void render(float x, float y, float width, float height, const Theme& theme);

    int selectedIndex() const { return selected_; }
    void select(int index);
    void selectNext();
    void selectPrev();

    // replace the entries wholesale (called when channel list changes)
    void setEntries(const std::vector<BufferEntry>& entries);
    const std::vector<BufferEntry>& entries() const { return entries_; }

    // right-click context menu feedback - "leave channel", "mark as read", etc.
    const std::string& rightClickedChannel() const { return right_clicked_channel_; }
    void clearRightClick() { right_clicked_channel_.clear(); }

private:
    std::vector<BufferEntry> entries_;
    int selected_ = 0;
    std::string right_clicked_channel_;
};

} // namespace conduit::ui
