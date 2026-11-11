#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "events/EventBus.hpp"
#include "models/BookToc.h"
#include "input/BookTocInputs.hpp"
#include <string>
#include <vector>

class BookTocService {
    public:
       BookTocService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       drogon::Task<std::vector<drogon_model::bookstore::BookToc>> getToc(const std::string& bookId, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<size_t> replaceToc(const std::string& bookId, const std::vector<TocEntry>& entries, drogon::orm::DbClientPtr tx = nullptr);

       drogon::Task<size_t> clearToc(const std::string& bookId, drogon::orm::DbClientPtr tx = nullptr);

    private:
       drogon::Task<> writeToc(drogon::orm::DbClientPtr client, const std::string& bookId, const std::vector<TocEntry>& entries);

       drogon::orm::DbClientPtr db(const drogon::orm::DbClientPtr& tx) const { return tx ? tx : dbClient_; }

       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
