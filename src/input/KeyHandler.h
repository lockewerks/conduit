#pragma once
#include <SDL.h>
#include <functional>
#include <unordered_map>
#include <string>

namespace conduit::input {

// keybinding system - maps key combos to action names
// supports modifiers (ctrl, alt, shift) and multi-key sequences (Alt+J, then number)
class KeyHandler {
public:
    using ActionCallback = std::function<void()>;

    void bind(const std::string& action, SDL_Keycode key, uint16_t mod = 0);
    void onAction(const std::string& action, ActionCallback cb);

    // call this from the SDL event loop
    // returns true if we consumed the key event
    bool handleKeyDown(const SDL_KeyboardEvent& key);

    // some built-in action names
    static constexpr const char* kSwitchNextBuffer = "switch_next_buffer";
    static constexpr const char* kSwitchPrevBuffer = "switch_prev_buffer";
    static constexpr const char* kNextUnread = "next_unread";
    static constexpr const char* kToggleBufferList = "toggle_buffer_list";
    static constexpr const char* kToggleNickList = "toggle_nick_list";
    static constexpr const char* kOpenSearch = "open_search";
    static constexpr const char* kOpenCommandPalette = "open_command_palette";
    static constexpr const char* kOpenThread = "open_thread";
    static constexpr const char* kOpenReactions = "open_reactions";
    static constexpr const char* kEditMessage = "edit_message";
    static constexpr const char* kDeleteMessage = "delete_message";
    static constexpr const char* kUploadFile = "upload_file";
    static constexpr const char* kOrgSwitcher = "org_switcher";
    static constexpr const char* kEscape = "escape";
    static constexpr const char* kPageUp = "page_up";
    static constexpr const char* kPageDown = "page_down";
    static constexpr const char* kScrollHome = "scroll_home";
    static constexpr const char* kScrollEnd = "scroll_end";

private:
    struct Binding {
        SDL_Keycode key;
        uint16_t mod;
        std::string action;
    };

    std::vector<Binding> bindings_;
    std::unordered_map<std::string, ActionCallback> callbacks_;
};

} // namespace conduit::input
