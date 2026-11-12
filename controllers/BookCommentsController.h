#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class BookCommentsController : public drogon::HttpController<BookCommentsController, false>
{
  public:
    explicit BookCommentsController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_VIA_REGEX(BookCommentsController::ListComments, API_V1 + "/books/([0-9a-fA-F-]{36})/comments", Get);
    ADD_METHOD_VIA_REGEX(BookCommentsController::AddComment, API_V1 + "/books/([0-9a-fA-F-]{36})/comments", Post, "JwtMiddleWare", "CommentRateLimit");
    ADD_METHOD_VIA_REGEX(BookCommentsController::UpdateComment, API_V1 + "/books/([0-9a-fA-F-]{36})/comments/([0-9a-fA-F-]{36})", Put, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(BookCommentsController::DeleteComment, API_V1 + "/books/([0-9a-fA-F-]{36})/comments/([0-9a-fA-F-]{36})", Delete, "JwtMiddleWare", "CommentAuthorOrAdminMiddleware");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListComments(HttpRequestPtr req, std::string bookId);
    Task<HttpResponsePtr> AddComment(HttpRequestPtr req, std::string bookId);
    Task<HttpResponsePtr> UpdateComment(HttpRequestPtr req, std::string bookId, std::string commentId);
    Task<HttpResponsePtr> DeleteComment(HttpRequestPtr req, std::string bookId, std::string commentId);

  private:
    Services* services_;
};
