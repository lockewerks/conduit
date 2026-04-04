#include "util/Platform.h"
#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace fs = std::filesystem;

namespace conduit::platform {

std::string getConfigDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\conduit";
    }
    return "."; // welp
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Library/Application Support/conduit"
                : ".";
#else
    // XDG spec or bust
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg) return std::string(xdg) + "/conduit";
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.config/conduit" : ".";
#endif
}

std::string getCacheDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\conduit\\cache";
    }
    return "./cache";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Library/Caches/conduit" : "./cache";
#else
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg) return std::string(xdg) + "/conduit";
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.cache/conduit" : "./cache";
#endif
}

std::string getDataDir() {
#ifdef _WIN32
    return getConfigDir() + "\\data";
#elif defined(__APPLE__)
    return getConfigDir() + "/data";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg) return std::string(xdg) + "/conduit";
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.local/share/conduit" : "./data";
#endif
}

bool ensureDir(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

void openURL(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = "open \"" + url + "\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + url + "\" &";
    std::system(cmd.c_str());
#endif
}

} // namespace conduit::platform
