#pragma once
#include <sodium.h>
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThread.h>
#include <stdexcept>
#include <string>

namespace security {

inline constexpr size_t MEMLIMIT = 19 * 1024 * 1024;

inline trantor::EventLoopThread* hashThread = nullptr;

inline void init() {
    if (sodium_init() < 0) throw std::runtime_error("libsodium failed to initialise");
    if (!hashThread) {
        hashThread = new trantor::EventLoopThread("PasswordHash");
        hashThread->run();
    }
}

inline std::string hash(const std::string& plain) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, plain.c_str(), plain.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          MEMLIMIT) != 0)
        throw std::runtime_error("out of memory while hashing password");
    return out;
}

inline bool verify(const std::string& plain, const std::string& stored) {
    return crypto_pwhash_str_verify(stored.c_str(), plain.c_str(), plain.size()) == 0;
}

inline bool needsRehash(const std::string& stored) {
    return crypto_pwhash_str_needs_rehash(stored.c_str(),
                                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                          MEMLIMIT) != 0;
}

inline drogon::Task<std::string> hashAsync(std::string plain) {
    trantor::EventLoop* origin = trantor::EventLoop::getEventLoopOfCurrentThread();
    std::string result = co_await drogon::queueInLoopCoro<std::string>(hashThread->getLoop(), [plain]() { return hash(plain); });
    if (origin) co_await drogon::switchThreadCoro(origin);
    co_return result;
}

inline drogon::Task<bool> verifyAsync(std::string plain, std::string stored) {
    trantor::EventLoop* origin = trantor::EventLoop::getEventLoopOfCurrentThread();
    bool result = co_await drogon::queueInLoopCoro<bool>(hashThread->getLoop(), [plain, stored]() { return verify(plain, stored); });
    if (origin) co_await drogon::switchThreadCoro(origin);
    co_return result;
}

}
