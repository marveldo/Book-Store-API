#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class BookTocController : public drogon::HttpController<BookTocController, false>
{
  public:
    explicit BookTocController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_VIA_REGEX(BookTocController::GetToc, API_V1 + "/books/([0-9a-fA-F-]{36})/toc", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(BookTocController::ReplaceToc, API_V1 + "/books/([0-9a-fA-F-]{36})/toc", Put, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    ADD_METHOD_VIA_REGEX(BookTocController::ClearToc, API_V1 + "/books/([0-9a-fA-F-]{36})/toc", Delete, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    METHOD_LIST_END

    Task<HttpResponsePtr> GetToc(HttpRequestPtr req, std::string bookId);
    Task<HttpResponsePtr> ReplaceToc(HttpRequestPtr req, std::string bookId);
    Task<HttpResponsePtr> ClearToc(HttpRequestPtr req, std::string bookId);

  private:
    Services* services_;
};
