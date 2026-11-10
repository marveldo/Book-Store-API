#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
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

       drogon::Task<drogon_model::bookstore::Books> createBook(const CreateBookInput& input);

       drogon::Task<drogon_model::bookstore::Books> updateBook(const std::string& id, const UpdateBookInput& input);

       drogon::Task<bool> deleteBook(const std::string& id);

       // ownership (user_books)
       drogon::Task<std::vector<drogon_model::bookstore::Books>> getBooksByUser(const std::string& userId);

       drogon::Task<drogon_model::bookstore::UserBooks> addBookToUser(const std::string& userId, const std::string& bookId);

       drogon::Task<bool> removeBookFromUser(const std::string& userId, const std::string& bookId);

    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
