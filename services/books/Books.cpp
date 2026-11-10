#include "Books.hpp"
#include <drogon/orm/CoroMapper.h>
#include "utils/Errors.hpp"

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

Task<vector<Books>> BookService::getAllBooks(size_t page, size_t perPage) {
    CoroMapper<Books> books(dbClient_);
    co_return co_await books.orderBy(Books::Cols::_created_at, SortOrder::DESC)
                            .paginate(page, perPage)
                            .findAll();
}

Task<Books> BookService::getBookById(const std::string& id) {
    CoroMapper<Books> books(dbClient_);
    try {
        co_return co_await books.findByPrimaryKey(id);
    } catch (const UnexpectedRows&) {
        throw NotFound("Book not found");
    }
}

Task<Books> BookService::createBook(const CreateBookInput& input) {
    CoroMapper<Books> books(dbClient_);

    Books book;
    book.setTitle(input.title);
    book.setAuthor(input.author);
    book.setPrice(input.price);
    if (input.isbn)          book.setIsbn(*input.isbn);
    if (input.description)   book.setDescription(*input.description);
    if (input.publishedYear) book.setPublishedYear(static_cast<short>(*input.publishedYear));
    if (input.cover)         book.setCoverKey(*input.cover);

    co_return co_await books.insert(book);
}

Task<Books> BookService::updateBook(const std::string& id, const UpdateBookInput& input) {
    CoroMapper<Books> books(dbClient_);
    auto book = co_await getBookById(id);

    if (input.title)         book.setTitle(*input.title);
    if (input.author)        book.setAuthor(*input.author);
    if (input.price)         book.setPrice(*input.price);
    if (input.isbn)          book.setIsbn(*input.isbn);
    if (input.description)   book.setDescription(*input.description);
    if (input.publishedYear) book.setPublishedYear(static_cast<short>(*input.publishedYear));
    if (input.cover)         book.setCoverKey(*input.cover);

    co_await books.update(book);
    co_return co_await getBookById(id);
}

Task<bool> BookService::deleteBook(const std::string& id) {
    CoroMapper<Books> books(dbClient_);
    co_return (co_await books.deleteByPrimaryKey(id)) > 0;
}

Task<vector<Books>> BookService::getBooksByUser(const std::string& userId) {
    auto rows = co_await dbClient_->execSqlCoro(
        "SELECT b.* FROM books b "
        "JOIN user_books ub ON ub.book_id = b.id "
        "WHERE ub.user_id = $1 "
        "ORDER BY ub.acquired_at DESC",
        userId);

    vector<Books> result;
    result.reserve(rows.size());
    for (const auto& row : rows) result.emplace_back(row);
    co_return result;
}

Task<UserBooks> BookService::addBookToUser(const std::string& userId, const std::string& bookId) {
    co_await getBookById(bookId);

    CoroMapper<UserBooks> userBooks(dbClient_);
    UserBooks link;
    link.setUserId(userId);
    link.setBookId(bookId);
    co_return co_await userBooks.insert(link);
}

Task<bool> BookService::removeBookFromUser(const std::string& userId, const std::string& bookId) {
    CoroMapper<UserBooks> userBooks(dbClient_);
    co_return (co_await userBooks.deleteByPrimaryKey(std::make_tuple(userId, bookId))) > 0;
}
