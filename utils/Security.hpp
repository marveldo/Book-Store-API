#pragma once
#include <sodium.h>
#include <stdexcept>
#include <string>

namespace security {

inline void init() {
    if (sodium_init() < 0) throw std::runtime_error("libsodium failed to initialise");
}

inline std::string hash(const std::string& plain) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, plain.c_str(), plain.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          32 * 1024 * 1024) != 0)
        throw std::runtime_error("out of memory while hashing password");
    return out;
}

inline bool verify(const std::string& plain, const std::string& stored) {
    return crypto_pwhash_str_verify(stored.c_str(), plain.c_str(), plain.size()) == 0;
}

inline bool needsRehash(const std::string& stored) {
    return crypto_pwhash_str_needs_rehash(stored.c_str(),
                                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                          32 * 1024 * 1024) != 0;
}

}
