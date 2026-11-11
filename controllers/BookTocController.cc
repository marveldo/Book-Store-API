#include "BookTocController.h"
#include "input/BookTocInputs.hpp"

Task<HttpResponsePtr> BookTocController::GetToc(HttpRequestPtr, std::string bookId) {
    co_await services_->bookService->getBookById(bookId);
    auto toc = co_await services_->bookTocService->getToc(bookId);
    co_return res::success(res::toJsonArray(toc));
}

Task<HttpResponsePtr> BookTocController::ReplaceToc(HttpRequestPtr req, std::string bookId) {
    auto parsed = parseReplaceToc(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    co_await services_->bookService->getBookById(bookId);
    co_await services_->bookTocService->replaceToc(bookId, std::get<ReplaceTocInput>(parsed).entries);

    auto toc = co_await services_->bookTocService->getToc(bookId);
    co_return res::success(res::toJsonArray(toc));
}

Task<HttpResponsePtr> BookTocController::ClearToc(HttpRequestPtr, std::string bookId) {
    co_await services_->bookService->getBookById(bookId);
    auto removed = co_await services_->bookTocService->clearToc(bookId);

    Json::Value data;
    data["removed"] = static_cast<Json::UInt64>(removed);
    co_return res::success(data);
}
