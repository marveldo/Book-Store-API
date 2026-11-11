#include "CommentAuthorOrAdminMiddleware.hpp"
#include "controllers/Api.hpp"
#include "utils/Errors.hpp"
#include <optional>

using namespace drogon;

Task<HttpResponsePtr> CommentAuthorOrAdminMiddleware::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    const std::string userId = req->attributes()->get<std::string>("id");
    if (userId.empty()) {
        co_return res::error(k401Unauthorized, "Not authenticated");
    }

    const auto& params = req->getRoutingParameters();
    if (params.size() < 2) {
        co_return res::error(k400BadRequest, "Book id and comment id are required");
    }

    std::string authorId;
    std::optional<AppError> failure;
    try {
        authorId = co_await services_->bookCommentService->getAuthorId(params[0], params[1]);
    } catch (const AppError& e) {
        failure = e;
    }
    if (failure) {
        co_return res::error(failure->status, failure->what());
    }

    if (authorId == userId) {
        co_return co_await next;
    }

    // The book's owner can moderate comments on their own book.
    std::optional<std::string> ownerId = co_await services_->bookService->getOwnerId(params[0]);
    if (ownerId && *ownerId == userId) {
        co_return co_await next;
    }
    if (co_await services_->userService->isAdmin(userId)) {
        co_return co_await next;
    }

    co_return res::error(k403Forbidden, "Only the comment's author, the book's owner or an admin can do this");
}
