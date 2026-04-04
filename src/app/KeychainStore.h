#pragma once
#include <string>
#include <optional>

namespace conduit {

// cross-platform keychain abstraction
// on windows: credential manager (DPAPI)
// on mac: keychain services
// on linux: libsecret (if available) or encrypted file fallback
class KeychainStore {
public:
    // store a secret in the OS keychain
    static bool store(const std::string& service, const std::string& account,
                      const std::string& secret);

    // retrieve a secret from the OS keychain
    static std::optional<std::string> retrieve(const std::string& service,
                                                const std::string& account);

    // delete a stored secret
    static bool remove(const std::string& service, const std::string& account);

    // check if a secret exists
    static bool exists(const std::string& service, const std::string& account);
};

} // namespace conduit
