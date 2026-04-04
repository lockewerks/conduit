#include "input/KeyHandler.h"

namespace conduit::input {

void KeyHandler::bind(const std::string& action, SDL_Keycode key, uint16_t mod) {
    bindings_.push_back({key, mod, action});
}

void KeyHandler::onAction(const std::string& action, ActionCallback cb) {
    callbacks_[action] = std::move(cb);
}

bool KeyHandler::handleKeyDown(const SDL_KeyboardEvent& key) {
    for (auto& binding : bindings_) {
        if (key.keysym.sym != binding.key) continue;

        // check modifiers. we only care about ctrl/alt/shift
        uint16_t pressed_mod = key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT);
        uint16_t required_mod = binding.mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT);

        if (pressed_mod != required_mod) continue;

        auto it = callbacks_.find(binding.action);
        if (it != callbacks_.end()) {
            it->second();
            return true;
        }
    }
    return false;
}

} // namespace conduit::input
