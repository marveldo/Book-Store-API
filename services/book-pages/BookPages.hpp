#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "events/EventBus.hpp"
#include "models/BookPages.h"
#include "input/BookPageInputs.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct PageContent {
    int32_t     pageNumber;
    std::string content;
};

class BookPageService {
    public:
       BookPageService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       drogon::Task<std::vector<drogon_model::bookstore::BookPages>> getPages(const std::string& bookId, size_t page, size_t perPage, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<size_t> countPages(const std::string& bookId, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<drogon_model::bookstore::BookPages> getPage(const std::string& bookId, int32_t pageNumber, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<drogon_model::bookstore::BookPages> createPage(const std::string& bookId, const CreatePageInput& input, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<size_t> createPages(const std::string& bookId, const std::vector<PageContent>& pages, drogon::orm::DbClientPtr tx = nullptr);

       // Deletes every page of the book, then inserts `pages`. Used when a PDF replaces the book's content.
       drogon::Task<size_t> replacePages(const std::string& bookId, const std::vector<PageContent>& pages, drogon::orm::DbClientPtr tx = nullptr);

       // Job ids of earlier PDF imports whose images ("books/<book>/<job>/...") the pages still use.
       drogon::Task<std::vector<std::string>> imageFolders(const std::string& bookId, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<drogon_model::bookstore::BookPages> updatePage(const std::string& bookId, int32_t pageNumber, const UpdatePageInput& input, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<bool> deletePage(const std::string& bookId, int32_t pageNumber, drogon::orm::DbClientPtr tx = nullptr);

       void queueExtraction(const std::string& jobId, const std::string& bookId, const std::string& path);

    private:
       drogon::orm::DbClientPtr db(const drogon::orm::DbClientPtr& tx) const { return tx ? tx : dbClient_; }

       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
