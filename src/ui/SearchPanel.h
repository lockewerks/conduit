#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <string>
#include <vector>

namespace conduit::ui {

class SearchPanel {
public:
    void open() { is_open_ = true; query_.clear(); results_.clear(); }
    void close() { is_open_ = false; }
    bool isOpen() const { return is_open_; }

    void setResults(const std::vector<slack::Message>& results) { results_ = results; }
    void render(float x, float y, float width, float height, const Theme& theme);

    // returns non-empty string when user submits a search query
    std::string pendingQuery();

private:
    bool is_open_ = false;
    std::string query_;
    char search_buf_[256] = {};
    std::vector<slack::Message> results_;
    bool submitted_ = false;
};

} // namespace conduit::ui
