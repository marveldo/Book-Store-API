#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <functional>
#include <optional>
#include "events/EventBus.hpp"
#include "models/Books.h"
#include "models/UserBooks.h"
#include "input/BookInputs.hpp"
#include <string>
#include <vector>

class BookService {
    public:
       BookService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       // catalogue
       drogon::Task<std::vector<drogon_model::bookstore::Books>> getAllBooks(size_t page, size_t perPage);

       drogon::Task<drogon_model::bookstore::Books> getBookById(const std::string& id);

       drogon::Task<drogon_model::bookstore::Books> createBook(const CreateBookInput& input, const std::string& ownerId);

       drogon::Task<drogon_model::bookstore::Books> updateBook(const std::string& id, const UpdateBookInput& input);

       drogon::Task<bool> deleteBook(const std::string& id);

       drogon::Task<std::optional<std::string>> getOwnerId(const std::string& id);

       // For PDF imports: page 1 becomes the cover only if the user hasn't set one.
       drogon::Task<bool> hasCover(const std::string& id);
       drogon::Task<bool> setCoverIfMissing(const std::string& id, const std::string& coverKey, drogon::orm::DbClientPtr tx = nullptr);

       // ownership (user_books)
       drogon::Task<std::vector<drogon_model::bookstore::Books>> getBooksByUser(const std::string& userId);

       drogon::Task<drogon_model::bookstore::UserBooks> addBookToUser(const std::string& userId, const std::string& bookId);

       drogon::Task<bool> removeBookFromUser(const std::string& userId, const std::string& bookId);

       drogon::Task<> withTransaction(std::function<drogon::Task<>(drogon::orm::DbClientPtr tx)> work);

    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
