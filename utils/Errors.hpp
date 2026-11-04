#pragma once
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/orm/Exception.h>
#include <trantor/utils/Logger.h>
#include <functional>
#include <stdexcept>
#include <string>
#include "controllers/Api.hpp"

struct AppError : std::runtime_error {
    drogon::HttpStatusCode status;
    AppError(drogon::HttpStatusCode s, const std::string& msg) : std::runtime_error(msg), status(s) {}
};

struct NotFound : AppError { explicit NotFound(const std::string& m) : AppError(drogon::k404NotFound, m) {} };
struct Conflict : AppError { explicit Conflict(const std::string& m) : AppError(drogon::k409Conflict, m) {} };
struct Forbidden : AppError { explicit Forbidden(const std::string& m) : AppError(drogon::k403Forbidden , m) {}};

inline void handleException(const std::exception&,
                            const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        throw;
    }catch (const AppError& e) {
        callback(res::error(e.status, e.what()));
    }catch (const drogon::orm::UnexpectedRows&) {
        callback(res::notFound());
    }catch (const drogon::orm::UniqueViolation&) {
        callback(res::error(drogon::k409Conflict, "Already exists"));
    }catch (const std::exception& e) {
        LOG_ERROR << req->path() << ": " << e.what();
        callback(res::error(drogon::k500InternalServerError, "Internal server error"));
    }
}
