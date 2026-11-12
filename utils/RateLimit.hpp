#pragma once
#include <drogon/HttpResponse.h>
#include <drogon/RateLimiter.h>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include "controllers/Api.hpp"

// Token buckets keyed by a string (an email, a user id), never by IP address: on Render the IP
// is the proxy's, and mobile networks put thousands of people behind one IP.
//
// Each key gets its own drogon::RateLimiter (token bucket). `allowed` requests can come in a
// burst; after that they refill at `allowed` per `per`. In memory and per server instance:
// checking costs well under a microsecond.
class KeyedRateLimiter {
public:
    KeyedRateLimiter(size_t allowed, std::chrono::seconds per) : allowed_(allowed), per_(per) {}

    bool allow(const std::string& key) {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(mutex_);   // one lock for the map and the bucket: RateLimiter isn't thread safe

        if (++calls_ % 1024 == 0) forgetIdle(now);

        auto it = buckets_.find(key);
        if (it == buckets_.end()) {
            // Drogon refills a little on every call (time always passes between two requests), so a
            // capacity of N lets exactly N through in a burst, then N more per `per`.
            auto limiter = drogon::RateLimiter::newRateLimiter(drogon::RateLimiterType::kTokenBucket, allowed_,
                                                               std::chrono::duration<double>(per_));
            it = buckets_.emplace(key, Bucket{std::move(limiter), now}).first;
        }
        it->second.lastUsed = now;
        return it->second.limiter->isAllowed();
    }

    // Roughly how long until one more request is allowed.
    long retryAfterSeconds() const {
        return std::max<long>(1, static_cast<long>(std::ceil(double(per_.count()) / double(allowed_))));
    }

private:
    struct Bucket {
        drogon::RateLimiterPtr limiter;
        std::chrono::steady_clock::time_point lastUsed;
    };

    // A bucket nobody has touched for a whole period is full again, the same as a new one:
    // drop it so the map doesn't grow with every email ever tried.
    void forgetIdle(std::chrono::steady_clock::time_point now) {
        std::erase_if(buckets_, [&](const auto& entry) { return now - entry.second.lastUsed > per_; });
    }

    size_t allowed_;
    std::chrono::seconds per_;
    std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
    size_t calls_ = 0;
};

inline drogon::HttpResponsePtr tooManyRequests(const KeyedRateLimiter& limiter, const std::string& message) {
    auto resp = res::error(drogon::k429TooManyRequests, message);
    resp->addHeader("Retry-After", std::to_string(limiter.retryAfterSeconds()));
    return resp;
}

// All the limits in one place.
namespace limits {
    using namespace std::chrono_literals;
    inline KeyedRateLimiter& login()      { static KeyedRateLimiter l(5, 15min);  return l; }   // per email
    inline KeyedRateLimiter& signup()     { static KeyedRateLimiter l(30, 1min);  return l; }   // everyone together
    inline KeyedRateLimiter& refresh()    { static KeyedRateLimiter l(10, 1min);  return l; }   // per user
    inline KeyedRateLimiter& user()       { static KeyedRateLimiter l(120, 1min); return l; }   // per user, every logged-in request
    inline KeyedRateLimiter& pdfUpload()  { static KeyedRateLimiter l(5, 1h);     return l; }   // per user
    inline KeyedRateLimiter& comment()    { static KeyedRateLimiter l(10, 1min);  return l; }   // per user
    inline KeyedRateLimiter& signature()  { static KeyedRateLimiter l(60, 1h);    return l; }   // per user
}

// The general per-user limit, for every route that needs a login. Runs after the route's
// middlewares (so JwtMiddleWare has set the user id) and before the handler.
void registerUserRateLimit();
