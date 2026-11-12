#include "TaskHandlers.hpp"
#include "TaskMiddlewares.hpp"
#include "utils/DbErrors.hpp"
#include "utils/Errors.hpp"
#include "queues/TaskErrors.hpp"
#include "utils/Media.hpp"
#include <exception>
#include <optional>
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

    // Cleanup that mustn't hide the real error: log and carry on.
    drogon::Task<> deleteImagesQuietly(CloudinaryService* cloud, std::string prefix) {
        try {
            co_await cloud->deleteByPrefix(prefix);
        } catch (const std::exception& e) {
            LOG_WARN << "extract_pdf: could not delete images under " << prefix << ": " << e.what();
        }
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

        // Pictures are named after the book and this job, so a failed import can be cleaned
        // up without touching the images of the version currently shown.
        const std::string folder = "books/" + bookId + "/" + jobId;
        const PdfImageOptions imageOptions{folder, [](const std::string& publicId) { return imageUrl(publicId); }};

        ExtractedPdf pdf;
        try {
            pdf = co_await services->pdfExtractor->extract(path, imageOptions, progressReporter(jobs, jobId));
        } catch (const PdfError& e) {
            throw PermanentTaskError(e.what());   // a broken PDF stays broken
        }

        CloudinaryService* cloud = services->cloudinaryService;
        std::optional<std::string> coverKey;
        bool coverUsed = false;
        std::vector<std::string> oldFolders;
        std::exception_ptr failure;
        try {
            // 1. Upload the pictures. The pages point at them, so they must exist before the pages are saved.
            std::vector<ImageUpload> uploads;
            for (const auto& image : pdf.images) uploads.push_back(ImageUpload{image.publicId, &image.data, image.mime});
            co_await cloud->uploadAll(std::move(uploads));

            // 2. Page 1 looked like a cover: it becomes the book's cover, unless the user already set one.
            bool needsCover = false;
            if (pdf.cover) needsCover = !(co_await services->bookService->hasCover(bookId));
            if (needsCover) {
                coverKey = "covers/" + drogon::utils::getUuid();
                std::vector<ImageUpload> cover{ImageUpload{*coverKey, &pdf.cover->data, pdf.cover->mime}};
                co_await cloud->uploadAll(std::move(cover));
            }

            // 3. Pages, table of contents and cover together, or not at all.
            co_await services->bookService->withTransaction([&](drogon::orm::DbClientPtr tx) -> drogon::Task<> {
                oldFolders = co_await services->bookPageService->imageFolders(bookId, tx);
                co_await services->bookPageService->replacePages(bookId, pdf.pages, tx);
                co_await services->bookTocService->replaceToc(bookId, pdf.toc, tx);
                if (coverKey) coverUsed = co_await services->bookService->setCoverIfMissing(bookId, *coverKey, tx);
            });
        } catch (...) {
            failure = std::current_exception();   // co_await isn't allowed inside catch, so clean up below
        }

        if (failure) {
            // Nothing points at this job's images now. Delete them, then report the real error.
            co_await deleteImagesQuietly(cloud, folder + "/");
            if (coverKey) co_await deleteImagesQuietly(cloud, *coverKey);
            try {
                std::rethrow_exception(failure);
            } catch (const NotFound&) {
                throw PermanentTaskError("The book was deleted before its PDF finished processing");
            } catch (const drogon::orm::Failure& e) {
                if (isForeignKeyViolation(e)) throw PermanentTaskError("The book was deleted before its PDF finished processing");
                throw;
            }
        }

        // The pages now use this job's images: the ones from earlier imports of this book can go.
        for (const auto& old : oldFolders) {
            if (old != jobId) co_await deleteImagesQuietly(cloud, "books/" + bookId + "/" + old + "/");
        }
        if (coverKey && !coverUsed) co_await deleteImagesQuietly(cloud, *coverKey);   // the user set a cover meanwhile

        removeFile(path);
        std::vector<std::string> warnings;
        if (pdf.cover) {
            warnings.push_back(coverUsed ? "Page 1 looked like a cover, so it was removed from the pages and set as the book's cover"
                                         : "Page 1 looked like a cover, so it was removed from the pages (the book already has a cover)");
        }
        warnings.insert(warnings.end(), pdf.warnings.begin(), pdf.warnings.end());
        co_await jobs->complete(jobId, static_cast<int>(pdf.pages.size()), warnings);
        co_await jobs->publish(jobId);
        co_await jobs->unlockBook(bookId, jobId);
    }, {timed("extract_pdf")},
    // Gave up (final attempt or PermanentTaskError): clean up so the book can be uploaded again.
    [services](const nlohmann::json& payload, const std::string& error) -> drogon::Task<> {
        removeFile(payload.value("path", ""));
        co_await services->jobService->setStatus(payload.value("job_id", ""), "failed", error);
        co_await services->jobService->publish(payload.value("job_id", ""));
        co_await services->jobService->unlockBook(payload.value("book_id", ""), payload.value("job_id", ""));
    },
    "pdf");   // its own lane: see Bootstrap
}
