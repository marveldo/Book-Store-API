#include "RateLimitMiddlewares.hpp"
#include "utils/RateLimit.hpp"
#include <drogon/drogon.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

using namespace drogon;

namespace {
    // A string field from the JSON body, or "" (the handler reports bad bodies itself).
    std::string bodyField(const HttpRequestPtr& req, const char* name) {
        auto body = nlohmann::json::parse(req->body(), nullptr, false);
        if (!body.is_object() || !body.contains(name) || !body[name].is_string()) return "";
        return body[name].get<std::string>();
    }

    // Set by JwtMiddleWare.
    std::string userId(const HttpRequestPtr& req) {
        return req->attributes()->find("id") ? req->attributes()->get<std::string>("id") : "";
    }

    Task<HttpResponsePtr> limitBy(KeyedRateLimiter& limiter, const std::string& key, const char* message,
                                  MiddlewareNextAwaiter&& next) {
        if (!key.empty() && !limiter.allow(key)) co_return tooManyRequests(limiter, message);
        co_return co_await next;
    }
}

Task<HttpResponsePtr> LoginRateLimit::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    std::string email = bodyField(req, "email");
    std::transform(email.begin(), email.end(), email.begin(), [](unsigned char c) { return std::tolower(c); });
    co_return co_await limitBy(limits::login(), email,
                               "Too many login attempts for this account. Try again in a few minutes.", std::move(next));
}

Task<HttpResponsePtr> SignupRateLimit::invoke(const HttpRequestPtr&, MiddlewareNextAwaiter&& next) {
    co_return co_await limitBy(limits::signup(), "everyone",
                               "Too many sign-ups right now. Try again in a moment.", std::move(next));
}

Task<HttpResponsePtr> RefreshRateLimit::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    // Keyed on the user id in the token, read without checking the signature: the handler checks
    // it. A forged id only gets a forged token rejected sooner; garbage shares one key.
    std::string key = "unreadable";
    try {
        const auto decoded = jwt::decode(bodyField(req, "refresh_token"));
        key = decoded.get_payload_json().at("user_id").get<std::string>();
    } catch (const std::exception&) {
    }
    co_return co_await limitBy(limits::refresh(), key, "Too many token refreshes. Try again in a minute.", std::move(next));
}

Task<HttpResponsePtr> PdfUploadRateLimit::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    co_return co_await limitBy(limits::pdfUpload(), userId(req),
                               "You've uploaded several PDFs in the last hour. Try again later.", std::move(next));
}

Task<HttpResponsePtr> CommentRateLimit::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    co_return co_await limitBy(limits::comment(), userId(req),
                               "You're commenting too fast. Wait a moment.", std::move(next));
}

Task<HttpResponsePtr> SignatureRateLimit::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    co_return co_await limitBy(limits::signature(), userId(req),
                               "Too many image uploads in the last hour. Try again later.", std::move(next));
}

void registerUserRateLimit() {
    // Pre-handling advice runs after the route's middlewares and before its handler, for every
    // route. Requests without a user id (public routes) pass straight through.
    app().registerPreHandlingAdvice([](const HttpRequestPtr& req, AdviceCallback&& reject, AdviceChainCallback&& pass) {
        const std::string id = userId(req);
        if (!id.empty() && !limits::user().allow(id)) {
            reject(tooManyRequests(limits::user(), "Too many requests. Slow down a little."));
            return;
        }
        pass();
    });
}
