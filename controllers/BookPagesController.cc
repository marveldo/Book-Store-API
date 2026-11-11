#include "BookPagesController.h"
#include "input/BookPageInputs.hpp"
#include "input/QueryInputs.hpp"
#include "settings/Settings.h"
#include <drogon/MultiPart.h>

Task<HttpResponsePtr> BookPagesController::ListPages(HttpRequestPtr req, std::string bookId) {
    auto query = parsePageQuery(req->getParameter("page"), req->getParameter("per_page"));
    if (auto* errors = std::get_if<std::vector<FieldError>>(&query)) {
        co_return res::badRequest(*errors);
    }
    const auto& page = std::get<PageInput>(query);

    auto pages = co_await services_->bookPageService->getPages(bookId, page.page, page.perPage);
    auto total = co_await services_->bookPageService->countPages(bookId);

    Json::Value data;
    data["pages"]      = res::toJsonArray(pages);
    data["total"]      = static_cast<Json::UInt64>(total);
    data["page"]       = static_cast<Json::UInt64>(page.page);
    data["per_page"]   = static_cast<Json::UInt64>(page.perPage);
    co_return res::success(data);
}

Task<HttpResponsePtr> BookPagesController::GetPage(HttpRequestPtr, std::string bookId, int pageNumber) {
    auto page = co_await services_->bookPageService->getPage(bookId, pageNumber);
    co_return res::success(page.toJson());
}

Task<HttpResponsePtr> BookPagesController::CreatePage(HttpRequestPtr req, std::string bookId) {
    auto parsed = parseCreatePage(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    auto page = co_await services_->bookPageService->createPage(bookId, std::get<CreatePageInput>(parsed));
    co_return res::success(page.toJson(), k201Created);
}

Task<HttpResponsePtr> BookPagesController::UpdatePage(HttpRequestPtr req, std::string bookId, int pageNumber) {
    auto parsed = parseUpdatePage(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    auto page = co_await services_->bookPageService->updatePage(bookId, pageNumber, std::get<UpdatePageInput>(parsed));
    co_return res::success(page.toJson());
}

Task<HttpResponsePtr> BookPagesController::DeletePage(HttpRequestPtr, std::string bookId, int pageNumber) {
    if (!co_await services_->bookPageService->deletePage(bookId, pageNumber)) {
        co_return res::notFound("Page not found");
    }
    co_return res::success(Json::Value());
}

Task<HttpResponsePtr> BookPagesController::UploadPdf(HttpRequestPtr req, std::string bookId) {
    co_await services_->bookService->getBookById(bookId);

    MultiPartParser parser;
    if (parser.parse(req) != 0 || parser.getFiles().size() != 1 || parser.getFiles()[0].getItemName() != "file") {
        co_return res::badRequest("Send exactly one PDF as multipart/form-data in a field named 'file'");
    }
    const HttpFile& file = parser.getFiles()[0];

    const size_t maxBytes = getSettings()->uploads.pdfMaxBytes;
    if (file.fileLength() > maxBytes) {
        co_return res::error(k413RequestEntityTooLarge, "PDF must be at most " + std::to_string(maxBytes / (1024 * 1024)) + " MB");
    }
    if (file.fileLength() < 5 || file.fileContent().substr(0, 5) != "%PDF-") {
        co_return res::badRequest("File is not a PDF");
    }

    const std::string userId = req->attributes()->get<std::string>("id");
    const std::string jobId  = drogon::utils::getUuid();

    // One PDF per book at a time, so two uploads can't overwrite each other's pages.
    const bool locked = co_await services_->jobService->lockBook(bookId, jobId);
    if (!locked) {
        co_return res::error(k409Conflict, "A PDF for this book is already being processed. Wait for it to finish.");
    }

    const std::string fileName = "pdfs/" + jobId + ".pdf";
    if (file.saveAs(fileName) != 0) {
        co_await services_->jobService->unlockBook(bookId, jobId);
        co_return res::error(k500InternalServerError, "Could not store the uploaded PDF");
    }

    co_await services_->jobService->create(jobId, bookId, userId);
    services_->bookPageService->queueExtraction(jobId, bookId, app().getUploadPath() + "/" + fileName);

    Json::Value data;
    data["job_id"]     = jobId;
    data["book_id"]    = bookId;
    data["status"]     = "queued";
    data["ws_url"]     = API_V1 + "/jobs/ws?job_id=" + jobId;   // the client adds &token=<access token>
    co_return res::success(data, k202Accepted);
}
