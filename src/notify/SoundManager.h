#pragma once

namespace conduit::notify {

// plays the system notification sound
// that's it. that's the whole class.
// we could use OpenAL or FMOD but that's a comical amount of overkill
// for going "ding" when someone @mentions you
class SoundManager {
public:
    void playNotification();

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

private:
    bool enabled_ = true;
};

} // namespace conduit::notify
