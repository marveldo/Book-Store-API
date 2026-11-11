#include "BookPages.hpp"
#include <drogon/orm/CoroMapper.h>
#include <algorithm>
#include "utils/Errors.hpp"
#include "utils/DbErrors.hpp"
#include "events/Events.hpp"
#include <nlohmann/json.hpp>

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

namespace {
    std::string clean(std::string content) {
        content.erase(std::remove(content.begin(), content.end(), '\0'), content.end());
        return content;
    }

    Criteria pageOf(const std::string& bookId, int32_t pageNumber) {
        return Criteria(BookPages::Cols::_book_id, CompareOperator::EQ, bookId) &&
               Criteria(BookPages::Cols::_page_number, CompareOperator::EQ, pageNumber);
    }
}

Task<vector<BookPages>> BookPageService::getPages(const std::string& bookId, size_t page, size_t perPage, DbClientPtr tx) {
    CoroMapper<BookPages> pages(db(tx));
    co_return co_await pages.orderBy(BookPages::Cols::_page_number, SortOrder::ASC)
                            .paginate(page, perPage)
                            .findBy(Criteria(BookPages::Cols::_book_id, CompareOperator::EQ, bookId));
}

Task<size_t> BookPageService::countPages(const std::string& bookId, DbClientPtr tx) {
    CoroMapper<BookPages> pages(db(tx));
    co_return co_await pages.count(Criteria(BookPages::Cols::_book_id, CompareOperator::EQ, bookId));
}

Task<BookPages> BookPageService::getPage(const std::string& bookId, int32_t pageNumber, DbClientPtr tx) {
    CoroMapper<BookPages> pages(db(tx));
    try {
        co_return co_await pages.findOne(pageOf(bookId, pageNumber));
    } catch (const UnexpectedRows&) {
        throw NotFound("Page not found");
    }
}

Task<BookPages> BookPageService::createPage(const std::string& bookId, const CreatePageInput& input, DbClientPtr tx) {
    CoroMapper<BookPages> pages(db(tx));

    BookPages page;
    page.setBookId(bookId);
    page.setPageNumber(input.pageNumber);
    page.setContent(clean(input.content));

    try {
        co_return co_await pages.insert(page);
    } catch (const Failure& e) {
        if (isUniqueViolation(e))     throw Conflict("Page " + std::to_string(input.pageNumber) + " already exists");
        if (isForeignKeyViolation(e)) throw NotFound("Book not found");
        throw;
    }
}

Task<size_t> BookPageService::createPages(const std::string& bookId, const vector<PageContent>& pages, DbClientPtr tx) {
    auto client = db(tx);
    for (size_t i = 0; i < pages.size(); i++) {
        co_await client->execSqlCoro(
            "INSERT INTO book_pages (book_id, page_number, content) VALUES ($1, $2, $3) "
            "ON CONFLICT (book_id, page_number) DO UPDATE SET content = EXCLUDED.content",
            bookId, pages[i].pageNumber, clean(pages[i].content));
    }
    co_return pages.size();
}

Task<size_t> BookPageService::replacePages(const std::string& bookId, const vector<PageContent>& pages, DbClientPtr tx) {
    auto client = db(tx);
    co_await client->execSqlCoro("DELETE FROM book_pages WHERE book_id = $1", bookId);

    // 50 pages per INSERT instead of one round trip per page: the rows go in as one JSON array.
    constexpr size_t BATCH = 50;
    for (size_t start = 0; start < pages.size(); start += BATCH) {
        nlohmann::json rows = nlohmann::json::array();
        for (size_t i = start; i < std::min(pages.size(), start + BATCH); i++) {
            rows.push_back({{"n", pages[i].pageNumber}, {"c", clean(pages[i].content)}});
        }
        co_await client->execSqlCoro(
            "INSERT INTO book_pages (book_id, page_number, content) "
            "SELECT $1::uuid, (r->>'n')::int, r->>'c' FROM json_array_elements($2::json) AS r",
            bookId, rows.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
    }
    co_return pages.size();
}

Task<BookPages> BookPageService::updatePage(const std::string& bookId, int32_t pageNumber, const UpdatePageInput& input, DbClientPtr tx) {
    CoroMapper<BookPages> pages(db(tx));
    auto page = co_await getPage(bookId, pageNumber, tx);
    page.setContent(clean(input.content));
    co_await pages.update(page);
    co_return co_await getPage(bookId, pageNumber, tx);
}

Task<bool> BookPageService::deletePage(const std::string& bookId, int32_t pageNumber, DbClientPtr tx) {
    CoroMapper<BookPages> pages(db(tx));
    co_return co_await pages.deleteBy(pageOf(bookId, pageNumber)) > 0;
}

void BookPageService::queueExtraction(const std::string& jobId, const std::string& bookId, const std::string& path) {
    events_->Publish(PDF_UPLOADED, PdfUploaded{jobId, bookId, path});
}
