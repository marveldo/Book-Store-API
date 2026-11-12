#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class BookPagesController : public drogon::HttpController<BookPagesController, false>
{
  public:
    explicit BookPagesController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_VIA_REGEX(BookPagesController::ListPages, API_V1 + "/books/([0-9a-fA-F-]{36})/pages", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(BookPagesController::GetPage, API_V1 + "/books/([0-9a-fA-F-]{36})/pages/([0-9]{1,6})", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(BookPagesController::CreatePage, API_V1 + "/books/([0-9a-fA-F-]{36})/pages", Post, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    ADD_METHOD_VIA_REGEX(BookPagesController::UpdatePage, API_V1 + "/books/([0-9a-fA-F-]{36})/pages/([0-9]{1,6})", Put, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    ADD_METHOD_VIA_REGEX(BookPagesController::DeletePage, API_V1 + "/books/([0-9a-fA-F-]{36})/pages/([0-9]{1,6})", Delete, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    ADD_METHOD_VIA_REGEX(BookPagesController::UploadPdf, API_V1 + "/books/([0-9a-fA-F-]{36})/pdf", Post, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    ADD_METHOD_VIA_REGEX(BookPagesController::ImageSignature, API_V1 + "/books/([0-9a-fA-F-]{36})/images/signature", Post, "JwtMiddleWare", "BookOwnerOrAdminMiddleware");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListPages(HttpRequestPtr req, std::string bookId);
    Task<HttpResponsePtr> GetPage(HttpRequestPtr req, std::string bookId, int pageNumber);
    Task<HttpResponsePtr> CreatePage(HttpRequestPtr req, std::string bookId);
    Task<HttpResponsePtr> UpdatePage(HttpRequestPtr req, std::string bookId, int pageNumber);
    Task<HttpResponsePtr> DeletePage(HttpRequestPtr req, std::string bookId, int pageNumber);
    Task<HttpResponsePtr> UploadPdf(HttpRequestPtr req, std::string bookId);
    // For the page builder: a signed Cloudinary upload for one image in this book's pages.
    Task<HttpResponsePtr> ImageSignature(HttpRequestPtr req, std::string bookId);

  private:
    Services* services_;
};
