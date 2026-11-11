#include "TaskHandlers.hpp"
#include "TaskMiddlewares.hpp"
#include "utils/DbErrors.hpp"
#include "utils/Errors.hpp"
#include "queues/TaskErrors.hpp"
#include <algorithm>
#include <filesystem>
#include <memory>

namespace {
    void removeFile(const std::string& path) {
        std::error_code ec;   // the error_code overload doesn't throw if the file is already gone
        std::filesystem::remove(path, ec);
    }

    // Publishes progress about every 5% instead of on every page.
    PdfProgress progressReporter(JobService* jobs, std::string jobId) {
        auto last = std::make_shared<int>(0);
        return [jobs, jobId, last](int done, int total) {
            const int step = std::max(1, total / 20);
            if (done == total || done - *last >= step) {
                *last = done;
                jobs->setProgress(jobId, done, total);
                jobs->publishAsync(jobId);
            }
        };
    }
}

void registerTaskHandlers(TaskQueue* queue, EmailService* email, Services* services) {
    queue->registerHandler("send_email", [email](const nlohmann::json& payload) -> drogon::Task<> {
        co_await email->send(payload.at("to").get<std::string>(),
                             payload.at("subject").get<std::string>(),
                             payload.at("template").get<std::string>(),
                             payload.value("data", nlohmann::json::object()));
    }, {timed("send_email")});

    queue->registerHandler("create_user_settings", [services](const nlohmann::json& payload) -> drogon::Task<> {
        try {
            co_await services->userSettingsService->createDefaultSettings(payload.at("user_id").get<std::string>());
        } catch (const drogon::orm::Failure& e) {
            if (!isUniqueViolation(e)) throw;
        }
    },{timed("create_user_settings")});

    // Reads an uploaded PDF into book_pages and book_toc. See BookPagesController::UploadPdf.
    queue->registerHandler("extract_pdf", [services](const nlohmann::json& payload) -> drogon::Task<> {
        const std::string jobId  = payload.at("job_id").get<std::string>();
        const std::string bookId = payload.at("book_id").get<std::string>();
        const std::string path   = payload.at("path").get<std::string>();
        JobService* jobs = services->jobService;

        // Render's disk is wiped on restart, but the task may still be in Redis.
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            throw PermanentTaskError("The uploaded PDF is no longer on the server (it probably restarted). Upload it again.");
        }

        co_await jobs->setStatus(jobId, "processing");
        co_await jobs->publish(jobId);

        ExtractedPdf pdf;
        try {
            pdf = co_await services->pdfExtractor->extract(path, progressReporter(jobs, jobId));
        } catch (const PdfError& e) {
            throw PermanentTaskError(e.what());   // a broken PDF stays broken
        }

        // All pages and the TOC are saved together or not at all.
        try {
            co_await services->bookService->withTransaction([&](drogon::orm::DbClientPtr tx) -> drogon::Task<> {
                co_await services->bookPageService->replacePages(bookId, pdf.pages, tx);
                co_await services->bookTocService->replaceToc(bookId, pdf.toc, tx);
            });
        } catch (const NotFound&) {
            throw PermanentTaskError("The book was deleted before its PDF finished processing");
        } catch (const drogon::orm::Failure& e) {
            if (isForeignKeyViolation(e)) throw PermanentTaskError("The book was deleted before its PDF finished processing");
            throw;
        }

        removeFile(path);
        co_await jobs->complete(jobId, static_cast<int>(pdf.pages.size()));
        co_await jobs->publish(jobId);
        co_await jobs->unlockBook(bookId, jobId);
    }, {timed("extract_pdf")},
    // Gave up (final attempt or PermanentTaskError): clean up so the book can be uploaded again.
    [services](const nlohmann::json& payload, const std::string& error) -> drogon::Task<> {
        removeFile(payload.value("path", ""));
        co_await services->jobService->setStatus(payload.value("job_id", ""), "failed", error);
        co_await services->jobService->publish(payload.value("job_id", ""));
        co_await services->jobService->unlockBook(payload.value("book_id", ""), payload.value("job_id", ""));
    });
}
