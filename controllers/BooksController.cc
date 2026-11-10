#include "BooksController.h"
#include "utils/MoneyJson.hpp"
#include "input/BookInputs.hpp"
#include "input/QueryInputs.hpp"

Task<HttpResponsePtr> BooksController::ListBooks(HttpRequestPtr req) {
    auto query = parsePageQuery(req->getParameter("page"), req->getParameter("per_page"));
    if (auto* errors = std::get_if<std::vector<FieldError>>(&query)) {
        co_return res::badRequest(*errors);
    }
    const auto& page = std::get<PageInput>(query);

    auto books = co_await services_->bookService->getAllBooks(page.page, page.perPage);
    co_return res::success(res::toJsonArray(books, bookJson));
}

Task<HttpResponsePtr> BooksController::GetBook(HttpRequestPtr, std::string id) {
    auto book = co_await services_->bookService->getBookById(id);
    co_return res::success(bookJson(book));
}

Task<HttpResponsePtr> BooksController::CreateBook(HttpRequestPtr req) {
    auto parsed = parseCreateBook(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    const auto& input = std::get<CreateBookInput>(parsed);

    if (input.cover && !co_await services_->cloudinaryService->exists(*input.cover)) {
        co_return res::badRequest(std::vector<FieldError>{{"cover", "no uploaded image found for this cover"}});
    }

    auto book = co_await services_->bookService->createBook(input);
    co_return res::success(bookJson(book), k201Created);
}

Task<HttpResponsePtr> BooksController::UpdateBook(HttpRequestPtr req, std::string id) {
    auto parsed = parseUpdateBook(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    const auto& input = std::get<UpdateBookInput>(parsed);

    if (input.cover && !co_await services_->cloudinaryService->exists(*input.cover)) {
        co_return res::badRequest(std::vector<FieldError>{{"cover", "no uploaded image found for this cover"}});
    }

    auto book = co_await services_->bookService->updateBook(id, input);
    co_return res::success(bookJson(book));
}

Task<HttpResponsePtr> BooksController::DeleteBook(HttpRequestPtr, std::string id) {
    if (!co_await services_->bookService->deleteBook(id)) {
        co_return res::notFound("Book not found");
    }
    co_return res::success(Json::Value());
}

Task<HttpResponsePtr> BooksController::MyBooks(HttpRequestPtr req) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto books = co_await services_->bookService->getBooksByUser(userId);
    co_return res::success(res::toJsonArray(books, bookJson));
}

Task<HttpResponsePtr> BooksController::CoverSignature(HttpRequestPtr) {
    const std::string publicId = "covers/" + drogon::utils::getUuid();
    UploadSignature signature = services_->cloudinaryService->signUpload(publicId);

    Json::Value data;
    data["upload_url"] = signature.uploadUrl;
    for (size_t i = 0; i < signature.fields.size(); i++) {
        data["fields"][signature.fields[i].first] = signature.fields[i].second;
    }
    co_return res::success(data);
}
