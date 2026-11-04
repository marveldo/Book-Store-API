#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class BooksController : public drogon::HttpController<BooksController, false>
{
  public:
    explicit BooksController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(BooksController::ListBooks, API_V1 + "/books", Get);
    ADD_METHOD_VIA_REGEX(BooksController::GetBook, API_V1 + "/books/([0-9a-fA-F-]{36})", Get);
    ADD_METHOD_TO(BooksController::CreateBook, API_V1 + "/books", Post, "JwtMiddleWare", "AdminMiddleware");
    ADD_METHOD_VIA_REGEX(BooksController::UpdateBook, API_V1 + "/books/([0-9a-fA-F-]{36})", Put, "JwtMiddleWare", "AdminMiddleware");
    ADD_METHOD_VIA_REGEX(BooksController::DeleteBook, API_V1 + "/books/([0-9a-fA-F-]{36})", Delete, "JwtMiddleWare", "AdminMiddleware");
    ADD_METHOD_TO(BooksController::MyBooks, API_V1 + "/me/books", Get, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListBooks(HttpRequestPtr req);
    Task<HttpResponsePtr> GetBook(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> CreateBook(HttpRequestPtr req);
    Task<HttpResponsePtr> UpdateBook(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> DeleteBook(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> MyBooks(HttpRequestPtr req);

  private:
    Services* services_;
};
