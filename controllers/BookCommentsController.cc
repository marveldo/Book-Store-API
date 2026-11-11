#include "BookCommentsController.h"
#include "input/CommentInputs.hpp"
#include "input/QueryInputs.hpp"

Task<HttpResponsePtr> BookCommentsController::ListComments(HttpRequestPtr req, std::string bookId) {
    auto query = parsePageQuery(req->getParameter("page"), req->getParameter("per_page"));
    if (auto* errors = std::get_if<std::vector<FieldError>>(&query)) {
        co_return res::badRequest(*errors);
    }
    const auto& page = std::get<PageInput>(query);

    co_await services_->bookService->getBookById(bookId);
    auto comments = co_await services_->bookCommentService->getComments(bookId, page.page, page.perPage);
    auto total    = co_await services_->bookCommentService->countComments(bookId);

    Json::Value list(Json::arrayValue);
    for (size_t i = 0; i < comments.size(); i++) list.append(comments[i].toJson());

    Json::Value data;
    data["comments"] = list;
    data["total"]    = static_cast<Json::UInt64>(total);
    data["page"]     = static_cast<Json::UInt64>(page.page);
    data["per_page"] = static_cast<Json::UInt64>(page.perPage);
    co_return res::success(data);
}

Task<HttpResponsePtr> BookCommentsController::AddComment(HttpRequestPtr req, std::string bookId) {
    auto parsed = parseComment(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    const std::string userId = req->attributes()->get<std::string>("id");
    auto comment = co_await services_->bookCommentService->addComment(bookId, userId, std::get<CommentInput>(parsed).content);
    co_return res::success(comment.toJson(), k201Created);
}

Task<HttpResponsePtr> BookCommentsController::UpdateComment(HttpRequestPtr req, std::string bookId, std::string commentId) {
    auto parsed = parseComment(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    const std::string userId = req->attributes()->get<std::string>("id");
    auto comment = co_await services_->bookCommentService->updateComment(bookId, commentId, userId, std::get<CommentInput>(parsed).content);
    co_return res::success(comment.toJson());
}

Task<HttpResponsePtr> BookCommentsController::DeleteComment(HttpRequestPtr, std::string bookId, std::string commentId) {
    if (!co_await services_->bookCommentService->deleteComment(bookId, commentId)) {
        co_return res::notFound("Comment not found");
    }
    co_return res::success(Json::Value());
}
