#pragma once
#include "ui/Theme.h"
#include <string>
#include <vector>
#include <functional>

namespace conduit::ui {

// ctrl+K fuzzy finder for channels, commands, and anything else
class CommandPalette {
public:
    struct Entry {
        std::string display;
        std::string value;
        std::string category; // "channel", "command", "user"
    };

    using SelectCallback = std::function<void(const Entry& entry)>;

    void open();
    void close();
    bool isOpen() const { return is_open_; }

    void setEntries(const std::vector<Entry>& entries) { all_entries_ = entries; }
    void setSelectCallback(SelectCallback cb) { select_cb_ = std::move(cb); }

    void render(float x, float y, float width, float height, const Theme& theme);

private:
    bool is_open_ = false;
    char filter_buf_[256] = {};
    std::vector<Entry> all_entries_;
    std::vector<Entry> filtered_;
    int selected_ = 0;
    SelectCallback select_cb_;

    void filterEntries();
    bool fuzzyMatch(const std::string& text, const std::string& filter) const;
};

} // namespace conduit::ui
