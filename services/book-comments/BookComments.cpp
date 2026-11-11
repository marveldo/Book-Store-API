#include "BookComments.hpp"
#include "utils/Errors.hpp"
#include "utils/DbErrors.hpp"

using namespace drogon::orm;
using drogon::Task;
using std::vector;

namespace {
    const std::string SELECT_COMMENT =
        "SELECT c.id, c.book_id, c.user_id, u.username, c.content, c.created_at::text AS created_at, c.updated_at::text AS updated_at "
        "FROM book_comments c JOIN users u ON u.id = c.user_id ";

    BookComment fromRow(const Row& row) {
        return BookComment{
            row["id"].as<std::string>(),
            row["book_id"].as<std::string>(),
            row["user_id"].as<std::string>(),
            row["username"].as<std::string>(),
            row["content"].as<std::string>(),
            row["created_at"].as<std::string>(),
            row["updated_at"].as<std::string>(),
        };
    }
}

Json::Value BookComment::toJson() const {
    Json::Value json;
    json["id"]               = id;
    json["book_id"]          = bookId;
    json["user"]["id"]       = userId;
    json["user"]["username"] = username;
    json["content"]          = content;
    json["created_at"]       = createdAt;
    json["updated_at"]       = updatedAt;
    json["edited"]           = createdAt != updatedAt;
    return json;
}

Task<vector<BookComment>> BookCommentService::getComments(const std::string& bookId, size_t page, size_t perPage) {
    auto result = co_await dbClient_->execSqlCoro(
        SELECT_COMMENT + "WHERE c.book_id = $1 ORDER BY c.created_at DESC LIMIT $2 OFFSET $3",
        bookId, static_cast<int64_t>(perPage), static_cast<int64_t>((page - 1) * perPage));

    vector<BookComment> comments;
    for (size_t i = 0; i < result.size(); i++) comments.push_back(fromRow(result[i]));
    co_return comments;
}

Task<size_t> BookCommentService::countComments(const std::string& bookId) {
    auto result = co_await dbClient_->execSqlCoro("SELECT count(*) AS total FROM book_comments WHERE book_id = $1", bookId);
    co_return static_cast<size_t>(result[0]["total"].as<int64_t>());
}

Task<BookComment> BookCommentService::getComment(const std::string& bookId, const std::string& commentId) {
    auto result = co_await dbClient_->execSqlCoro(SELECT_COMMENT + "WHERE c.id = $1 AND c.book_id = $2", commentId, bookId);
    if (result.empty()) throw NotFound("Comment not found");
    co_return fromRow(result[0]);
}

Task<std::string> BookCommentService::getAuthorId(const std::string& bookId, const std::string& commentId) {
    auto result = co_await dbClient_->execSqlCoro("SELECT user_id FROM book_comments WHERE id = $1 AND book_id = $2", commentId, bookId);
    if (result.empty()) throw NotFound("Comment not found");
    co_return result[0]["user_id"].as<std::string>();
}

Task<BookComment> BookCommentService::addComment(const std::string& bookId, const std::string& userId, const std::string& content) {
    std::string id;
    try {
        auto result = co_await dbClient_->execSqlCoro(
            "INSERT INTO book_comments (book_id, user_id, content) VALUES ($1, $2, $3) RETURNING id",
            bookId, userId, content);
        id = result[0]["id"].as<std::string>();
    } catch (const Failure& e) {
        if (isForeignKeyViolation(e)) throw NotFound("Book not found");
        throw;
    }
    co_return co_await getComment(bookId, id);
}

Task<BookComment> BookCommentService::updateComment(const std::string& bookId, const std::string& commentId, const std::string& userId, const std::string& content) {
    auto authorId = co_await getAuthorId(bookId, commentId);
    if (authorId != userId) throw Forbidden("You can only edit your own comments");

    co_await dbClient_->execSqlCoro("UPDATE book_comments SET content = $1 WHERE id = $2", content, commentId);
    co_return co_await getComment(bookId, commentId);
}

Task<bool> BookCommentService::deleteComment(const std::string& bookId, const std::string& commentId) {
    auto result = co_await dbClient_->execSqlCoro("DELETE FROM book_comments WHERE id = $1 AND book_id = $2", commentId, bookId);
    co_return result.affectedRows() > 0;
}
