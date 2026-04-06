#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include "ui/Theme.h"

namespace conduit::ui {

struct BufferEntry {
    std::string name;
    bool is_active = false;
    bool has_unread = false;
    int unread_count = 0;
    bool is_dm = false;
    bool is_separator = false;      // org header
    bool is_section_header = false;  // "Channels", "Direct Messages"
    std::string channel_id;
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

    // right-click context menu feedback
    const std::string& rightClickedChannel() const { return right_clicked_channel_; }
    void clearRightClick() { right_clicked_channel_.clear(); }

private:
    std::vector<BufferEntry> entries_;
    int selected_ = 0;
    std::string right_clicked_channel_;
    std::unordered_set<std::string> collapsed_sections_;

    bool isSectionCollapsed(const std::string& name) const {
        return collapsed_sections_.count(name) > 0;
    }
    void toggleSection(const std::string& name) {
        if (collapsed_sections_.count(name)) collapsed_sections_.erase(name);
        else collapsed_sections_.insert(name);
    }
};

} // namespace conduit::ui
