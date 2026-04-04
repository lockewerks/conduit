#pragma once
#include "ui/Theme.h"
#include "ui/TitleBar.h"
#include "ui/BufferList.h"
#include "ui/BufferView.h"
#include "ui/NickList.h"
#include "ui/InputBar.h"
#include "ui/StatusBar.h"

namespace conduit::ui {

// layout config - all the knobs for panel sizing
struct LayoutConfig {
    float buffer_list_width = 180.0f;
    float nick_list_width = 160.0f;
    float input_bar_height = 28.0f;
    float status_bar_height = 20.0f;
    float title_bar_height = 24.0f;

    bool show_buffer_list = true;
    bool show_nick_list = true;
};

class UIManager {
public:
    UIManager();

    void render();

    // sidebar toggles (F5/F6 and F7/F8 per weechat convention)
    void toggleBufferList() { layout_.show_buffer_list = !layout_.show_buffer_list; }
    void toggleNickList() { layout_.show_nick_list = !layout_.show_nick_list; }

    Theme& theme() { return theme_; }
    LayoutConfig& layout() { return layout_; }
    InputBar& inputBar() { return input_bar_; }
    BufferList& bufferList() { return buffer_list_; }

private:
    Theme theme_;
    LayoutConfig layout_;

    TitleBar title_bar_;
    BufferList buffer_list_;
    BufferView buffer_view_;
    NickList nick_list_;
    InputBar input_bar_;
    StatusBar status_bar_;
};

} // namespace conduit::ui
