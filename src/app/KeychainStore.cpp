#include "app/KeychainStore.h"
#include "util/Logger.h"

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

namespace conduit {

#ifdef _WIN32

// windows credential manager - surprisingly not terrible to work with
bool KeychainStore::store(const std::string& service, const std::string& account,
                          const std::string& secret) {
    std::string target = service + "/" + account;

    CREDENTIALA cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<char*>(target.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = const_cast<char*>(account.c_str());

    if (CredWriteA(&cred, 0)) {
        LOG_DEBUG("stored credential for " + target);
        return true;
    }

    LOG_ERROR("failed to store credential for " + target + " (error " +
              std::to_string(GetLastError()) + ")");
    return false;
}

std::optional<std::string> KeychainStore::retrieve(const std::string& service,
                                                    const std::string& account) {
    std::string target = service + "/" + account;
    PCREDENTIALA cred = nullptr;

    if (CredReadA(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        std::string result(reinterpret_cast<char*>(cred->CredentialBlob),
                           cred->CredentialBlobSize);
        CredFree(cred);
        return result;
    }

    return std::nullopt;
}

bool KeychainStore::remove(const std::string& service, const std::string& account) {
    std::string target = service + "/" + account;
    return CredDeleteA(target.c_str(), CRED_TYPE_GENERIC, 0) != 0;
}

bool KeychainStore::exists(const std::string& service, const std::string& account) {
    return retrieve(service, account).has_value();
}

#elif defined(__APPLE__)

// macOS keychain - we'll implement this properly when we get there
// for now just log and fail gracefully

bool KeychainStore::store(const std::string& service, const std::string& account,
                          const std::string& secret) {
    LOG_WARN("macOS keychain not implemented yet, token won't persist");
    return false;
}

std::optional<std::string> KeychainStore::retrieve(const std::string& service,
                                                    const std::string& account) {
    return std::nullopt;
}

bool KeychainStore::remove(const std::string&, const std::string&) { return false; }
bool KeychainStore::exists(const std::string&, const std::string&) { return false; }

#else

// linux - would use libsecret here but that's a whole thing
// fallback to... nothing. for now.

bool KeychainStore::store(const std::string& service, const std::string& account,
                          const std::string& secret) {
    LOG_WARN("linux keychain not implemented yet, token won't persist");
    return false;
}

std::optional<std::string> KeychainStore::retrieve(const std::string& service,
                                                    const std::string& account) {
    return std::nullopt;
}

bool KeychainStore::remove(const std::string&, const std::string&) { return false; }
bool KeychainStore::exists(const std::string&, const std::string&) { return false; }

#endif

} // namespace conduit
