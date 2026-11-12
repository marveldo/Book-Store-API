#pragma once
#include <drogon/HttpMiddleware.h>

// Rate limits for single routes (the limits themselves are in utils/RateLimit.hpp).
// Add one to a route's middleware list, e.g. "JwtMiddleWare", "PdfUploadRateLimit", ...
// The per-user ones go after JwtMiddleWare, which sets the user id they key on.

// Login: per email being logged into, before any password hashing.
class LoginRateLimit : public drogon::HttpCoroMiddleware<LoginRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

// Sign up: one shared allowance for everyone (there's no account yet to key on).
class SignupRateLimit : public drogon::HttpCoroMiddleware<SignupRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

// Token refresh: per user id in the refresh token.
class RefreshRateLimit : public drogon::HttpCoroMiddleware<RefreshRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

// PDF imports: per user.
class PdfUploadRateLimit : public drogon::HttpCoroMiddleware<PdfUploadRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

// New comments: per user.
class CommentRateLimit : public drogon::HttpCoroMiddleware<CommentRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

// Cloudinary upload signatures (covers, builder images): per user.
class SignatureRateLimit : public drogon::HttpCoroMiddleware<SignatureRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};
