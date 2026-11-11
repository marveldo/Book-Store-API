#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include "events/EventBus.hpp"
#include <string>
#include <vector>

struct BookComment {
    std::string id;
    std::string bookId;
    std::string userId;
    std::string username;
    std::string content;
    std::string createdAt;
    std::string updatedAt;

    Json::Value toJson() const;
};

class BookCommentService {
    public:
       BookCommentService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       drogon::Task<std::vector<BookComment>> getComments(const std::string& bookId, size_t page, size_t perPage);

       drogon::Task<size_t> countComments(const std::string& bookId);

       drogon::Task<BookComment> getComment(const std::string& bookId, const std::string& commentId);

       drogon::Task<std::string> getAuthorId(const std::string& bookId, const std::string& commentId);

       drogon::Task<BookComment> addComment(const std::string& bookId, const std::string& userId, const std::string& content);

       drogon::Task<BookComment> updateComment(const std::string& bookId, const std::string& commentId, const std::string& userId, const std::string& content);

       drogon::Task<bool> deleteComment(const std::string& bookId, const std::string& commentId);

    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
