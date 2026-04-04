#pragma once
#include <string>

namespace conduit::notify {

enum class Urgency {
    Low,      // background chatter
    Normal,   // regular messages in joined channels
    High,     // direct mentions, DMs
};

// cross-platform desktop notification dispatch
// on windows we use MessageBeep for now because WinToast is a whole thing
// on linux we shell out to notify-send because life is too short for libnotify
// on mac we shell out to osascript because apple makes everything harder
class NotificationManager {
public:
    void notify(const std::string& title, const std::string& body,
                Urgency urgency = Urgency::Normal);

    // DND mode - silence everything
    void setDoNotDisturb(bool enabled) { dnd_ = enabled; }
    bool isDoNotDisturb() const { return dnd_; }

    // let the user disable desktop notifications entirely
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

private:
    bool dnd_ = false;
    bool enabled_ = true;

    void notifyWindows(const std::string& title, const std::string& body, Urgency urgency);
    void notifyLinux(const std::string& title, const std::string& body, Urgency urgency);
    void notifyMac(const std::string& title, const std::string& body, Urgency urgency);
};

} // namespace conduit::notify
