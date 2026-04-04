#include "notify/NotificationManager.h"
#include "util/Platform.h"
#include "util/Logger.h"
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace conduit::notify {

void NotificationManager::notify(const std::string& title, const std::string& body,
                                  Urgency urgency) {
    if (!enabled_ || dnd_) return;

    LOG_DEBUG("notification: [" + title + "] " + body);

    switch (platform::currentOS()) {
        case platform::OS::Windows:
            notifyWindows(title, body, urgency);
            break;
        case platform::OS::Linux:
            notifyLinux(title, body, urgency);
            break;
        case platform::OS::macOS:
            notifyMac(title, body, urgency);
            break;
        default:
            // running on a toaster? just log it
            break;
    }
}

void NotificationManager::notifyWindows([[maybe_unused]] const std::string& title,
                                         [[maybe_unused]] const std::string& body,
                                         [[maybe_unused]] Urgency urgency) {
#ifdef _WIN32
    // MessageBeep is about as sophisticated as we get without pulling in COM nonsense
    // TODO: proper toast notifications via WinToast when we hate ourselves enough to set it up
    UINT beep_type = MB_OK;
    switch (urgency) {
        case Urgency::Low:    beep_type = MB_OK; break;
        case Urgency::Normal: beep_type = MB_ICONINFORMATION; break;
        case Urgency::High:   beep_type = MB_ICONEXCLAMATION; break;
    }
    MessageBeep(beep_type);
#endif
}

void NotificationManager::notifyLinux(const std::string& title, const std::string& body,
                                       Urgency urgency) {
    // notify-send is available on basically every linux desktop
    // if it's not, the user is probably running a tiling WM and has their own setup
    std::string urgency_str;
    switch (urgency) {
        case Urgency::Low:    urgency_str = "low"; break;
        case Urgency::Normal: urgency_str = "normal"; break;
        case Urgency::High:   urgency_str = "critical"; break;
    }

    // escape single quotes in title and body because shell injection is a fun story to tell
    auto escape = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        return out;
    };

    std::string cmd = "notify-send -u " + urgency_str +
                      " -a Conduit '" + escape(title) + "' '" + escape(body) + "' &";
    std::system(cmd.c_str());
}

void NotificationManager::notifyMac(const std::string& title, const std::string& body,
                                     [[maybe_unused]] Urgency urgency) {
    // osascript is janky but it works without any dependencies
    auto escape = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else out += c;
        }
        return out;
    };

    std::string cmd = "osascript -e 'display notification \"" + escape(body) +
                      "\" with title \"" + escape(title) + "\"' &";
    std::system(cmd.c_str());
}

} // namespace conduit::notify
