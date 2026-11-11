#include "BookToc.hpp"
#include <drogon/orm/CoroMapper.h>
#include <stdexcept>
#include "utils/Errors.hpp"
#include "utils/DbErrors.hpp"
#include "utils/DbTransaction.hpp"

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

Task<vector<BookToc>> BookTocService::getToc(const std::string& bookId, DbClientPtr tx) {
    CoroMapper<BookToc> toc(db(tx));
    co_return co_await toc.orderBy(BookToc::Cols::_position, SortOrder::ASC)
                          .findBy(Criteria(BookToc::Cols::_book_id, CompareOperator::EQ, bookId));
}

Task<size_t> BookTocService::replaceToc(const std::string& bookId, const vector<TocEntry>& entries, DbClientPtr tx) {
    if (tx) {
        co_await writeToc(tx, bookId, entries);
        co_return entries.size();
    }

    auto own = co_await dbClient_->newTransactionCoro();
    try {
        co_await writeToc(own, bookId, entries);
    } catch (...) {
        own->rollback();
        throw;
    }

    if (!co_await CommitAwaiter(std::move(own))) throw std::runtime_error("Failed to commit table of contents");
    co_return entries.size();
}

Task<size_t> BookTocService::clearToc(const std::string& bookId, DbClientPtr tx) {
    CoroMapper<BookToc> toc(db(tx));
    co_return co_await toc.deleteBy(Criteria(BookToc::Cols::_book_id, CompareOperator::EQ, bookId));
}

Task<> BookTocService::writeToc(DbClientPtr client, const std::string& bookId, const vector<TocEntry>& entries) {
    co_await client->execSqlCoro("DELETE FROM book_toc WHERE book_id = $1", bookId);

    try {
        for (size_t i = 0; i < entries.size(); i++) {
            co_await client->execSqlCoro(
                "INSERT INTO book_toc (book_id, title, level, page_number, position) VALUES ($1, $2, $3, $4, $5)",
                bookId, entries[i].title, entries[i].level, entries[i].pageNumber, static_cast<int32_t>(i));
        }
    } catch (const Failure& e) {
        if (isForeignKeyViolation(e)) throw NotFound("Book not found");
        throw;
    }
}
