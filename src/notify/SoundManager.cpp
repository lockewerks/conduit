#include "notify/SoundManager.h"
#include "util/Platform.h"
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace conduit::notify {

void SoundManager::playNotification() {
    if (!enabled_) return;

    switch (platform::currentOS()) {
        case platform::OS::Windows:
#ifdef _WIN32
            // the classic windows "hey, look over here" sound
            MessageBeep(MB_ICONINFORMATION);
#endif
            break;

        case platform::OS::macOS:
            // afplay is built into macOS, Submarine is a nice subtle sound
            // if someone deleted their system sounds that's their problem
            std::system("afplay /System/Library/Sounds/Submarine.aiff &");
            break;

        case platform::OS::Linux:
            // paplay works with PulseAudio/PipeWire, which is most desktops these days
            // freedesktop sound theme has a standard notification sound
            std::system("paplay /usr/share/sounds/freedesktop/stereo/message-new-instant.oga 2>/dev/null &");
            break;

        default:
            // no sound for you
            break;
    }
}

} // namespace conduit::notify
