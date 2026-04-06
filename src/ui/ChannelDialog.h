#pragma once
#include "ui/Theme.h"
#include <string>
#include <functional>
#include <cstring>

namespace conduit::ui {

class ChannelDialog {
public:
    using CreateCallback = std::function<bool(const std::string& name, bool is_private)>;

    void open();
    void close() { is_open_ = false; }
    bool isOpen() const { return is_open_; }

    void setCreateCallback(CreateCallback cb) { create_cb_ = std::move(cb); }
    void render(const Theme& theme);

private:
    bool is_open_ = false;
    char name_buf_[128] = {};
    char purpose_buf_[256] = {};
    bool is_private_ = false;
    bool focus_name_ = false;
    CreateCallback create_cb_;
};

} // namespace conduit::ui
