#pragma once
#include "ui/Theme.h"
#include <string>
#include <vector>
#include <functional>

namespace conduit::ui {

// emoji picker popup with search
// no, we don't render actual unicode glyphs. this is a terminal-brained app.
// we show :emoji_name: and call it a day.
class EmojiPicker {
public:
    using SelectCallback = std::function<void(const std::string& emoji_name)>;

    void open(float anchor_x, float anchor_y);
    void close();
    bool isOpen() const { return is_open_; }

    void setEmojis(const std::vector<std::string>& emoji_names) { all_emojis_ = emoji_names; }
    void setSelectCallback(SelectCallback cb) { select_cb_ = std::move(cb); }

    void render(const Theme& theme);

private:
    bool is_open_ = false;
    float anchor_x_ = 0;
    float anchor_y_ = 0;
    char filter_buf_[128] = {};
    std::vector<std::string> all_emojis_;
    std::vector<std::string> filtered_;
    int selected_ = 0;
    SelectCallback select_cb_;

    // how many emojis per row in the grid
    static constexpr int kGridCols = 8;

    void filterEmojis();
};

} // namespace conduit::ui
