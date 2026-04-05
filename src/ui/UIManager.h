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
#include "ui/EmojiPicker.h"
#include "ui/FilePreview.h"

namespace conduit::ui {

struct LayoutConfig {
    float buffer_list_width = 180.0f;
    float nick_list_width = 160.0f;
    float input_bar_height = 28.0f;
    float status_bar_height = 20.0f;
    float title_bar_height = 24.0f;
    float thread_panel_width = 350.0f;

    // min/max constraints for draggable panes
    float buffer_list_min = 100.0f;
    float buffer_list_max = 400.0f;
    float nick_list_min = 80.0f;
    float nick_list_max = 350.0f;
    float thread_panel_min = 200.0f;
    float thread_panel_max = 600.0f;

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

    TitleBar& titleBar() { return title_bar_; }
    BufferList& bufferList() { return buffer_list_; }
    BufferView& bufferView() { return buffer_view_; }
    NickList& nickList() { return nick_list_; }
    InputBar& inputBar() { return input_bar_; }
    StatusBar& statusBar() { return status_bar_; }
    ThreadPanel& threadPanel() { return thread_panel_; }
    SearchPanel& searchPanel() { return search_panel_; }
    CommandPalette& commandPalette() { return command_palette_; }
    EmojiPicker& emojiPicker() { return emoji_picker_; }
    FilePreview& filePreview() { return file_preview_; }

    // right-click context menu results (checked by Application each frame)
    bool wantsPasteImage() const { return wants_paste_image_; }
    bool wantsPasteText() const { return wants_paste_text_; }
    void clearPasteImage() { wants_paste_image_ = false; }
    void clearPasteText() { wants_paste_text_ = false; }

private:
    bool wants_paste_image_ = false;
    bool wants_paste_text_ = false;
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
    EmojiPicker emoji_picker_;
    FilePreview file_preview_;

    // splitter drag state
    bool dragging_left_splitter_ = false;
    bool dragging_right_splitter_ = false;
    bool dragging_thread_splitter_ = false;

    // draw a vertical splitter handle and return true if it's being dragged
    bool verticalSplitter(const char* id, float x, float y, float height, float* width_to_adjust,
                          float min_w, float max_w, bool drag_left = false);

    // track the last window size so we can scale panes proportionally on resize
    float last_win_w_ = 0.0f;
};

} // namespace conduit::ui
