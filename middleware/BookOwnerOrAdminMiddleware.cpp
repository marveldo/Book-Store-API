#include "BookOwnerOrAdminMiddleware.hpp"
#include "controllers/Api.hpp"
#include "utils/Errors.hpp"
#include <optional>

using namespace drogon;

Task<HttpResponsePtr> BookOwnerOrAdminMiddleware::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    const std::string userId = req->attributes()->get<std::string>("id");
    if (userId.empty()) {
        co_return res::error(k401Unauthorized, "Not authenticated");
    }

    const auto& params = req->getRoutingParameters();
    if (params.empty()) {
        co_return res::error(k400BadRequest, "Book id is required");
    }

    std::optional<std::string> ownerId;
    std::optional<AppError> failure;
    try {
        ownerId = co_await services_->bookService->getOwnerId(params[0]);
    } catch (const AppError& e) {
        failure = e;
    }
    if (failure) {
        co_return res::error(failure->status, failure->what());
    }

    if (ownerId && *ownerId == userId) {
        co_return co_await next;
    }
    if (co_await services_->userService->isAdmin(userId)) {
        co_return co_await next;
    }

    co_return res::error(k403Forbidden, "Only the book's owner or an admin can do this");
}
