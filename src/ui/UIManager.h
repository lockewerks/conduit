#pragma once
#include "ui/Theme.h"
#include "ui/TitleBar.h"
#include "ui/BufferList.h"
#include "ui/BufferView.h"
#include "ui/NickList.h"
#include "ui/InputBar.h"
#include "ui/StatusBar.h"
#include "ui/ThreadPanel.h"
#include "ui/SearchPanel.h"
#include "ui/CommandPalette.h"

namespace conduit::ui {

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

    void toggleBufferList() { layout_.show_buffer_list = !layout_.show_buffer_list; }
    void toggleNickList() { layout_.show_nick_list = !layout_.show_nick_list; }

    Theme& theme() { return theme_; }
    LayoutConfig& layout() { return layout_; }

    // all the components, exposed so Application can wire them up
    TitleBar& titleBar() { return title_bar_; }
    BufferList& bufferList() { return buffer_list_; }
    BufferView& bufferView() { return buffer_view_; }
    NickList& nickList() { return nick_list_; }
    InputBar& inputBar() { return input_bar_; }
    StatusBar& statusBar() { return status_bar_; }
    ThreadPanel& threadPanel() { return thread_panel_; }
    SearchPanel& searchPanel() { return search_panel_; }
    CommandPalette& commandPalette() { return command_palette_; }

private:
    Theme theme_;
    LayoutConfig layout_;

    TitleBar title_bar_;
    BufferList buffer_list_;
    BufferView buffer_view_;
    NickList nick_list_;
    InputBar input_bar_;
    StatusBar status_bar_;
    ThreadPanel thread_panel_;
    SearchPanel search_panel_;
    CommandPalette command_palette_;
};

} // namespace conduit::ui
