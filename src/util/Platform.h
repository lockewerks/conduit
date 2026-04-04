#pragma once
#include <string>

// os detection so we don't have to pepper #ifdefs everywhere like animals
namespace conduit::platform {

enum class OS { Windows, Linux, macOS, Unknown };

constexpr OS currentOS() {
#if defined(_WIN32)
    return OS::Windows;
#elif defined(__APPLE__)
    return OS::macOS;
#elif defined(__linux__)
    return OS::Linux;
#else
    return OS::Unknown;
#endif
}

// get the config directory (~/.config/conduit on linux, AppData on windows, etc)
std::string getConfigDir();

// get the cache directory
std::string getCacheDir();

// get the data directory (for sqlite dbs)
std::string getDataDir();

// make sure a directory exists, create it if it doesn't
bool ensureDir(const std::string& path);

// open a URL in the default browser because we're not rendering HTML, we're not crazy
void openURL(const std::string& url);

} // namespace conduit::platform
