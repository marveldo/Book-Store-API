#include "Payments.hpp"
#include <drogon/orm/CoroMapper.h>
#include "models/Orders.h"
#include <optional>
#include "utils/Errors.hpp"

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

Task<vector<Payments>> PaymentService::getPaymentsByOrder(const std::string& orderId) {
    CoroMapper<Payments> payments(dbClient_);
    co_return co_await payments.findBy(Criteria(Payments::Cols::_order_id, CompareOperator::EQ, orderId));
}

Task<vector<Payments>> PaymentService::getPaymentsByUser(const std::string& userId) {
    CoroMapper<Payments> payments(dbClient_);
    co_return co_await payments.findBy(Criteria(Payments::Cols::_user_id, CompareOperator::EQ, userId));
}

Task<Payments> PaymentService::getPaymentById(const std::string& id) {
    CoroMapper<Payments> payments(dbClient_);
    try {
        co_return co_await payments.findByPrimaryKey(id);
    } catch (const UnexpectedRows&) {
        throw NotFound("Payment not found");
    }
}

Task<Payments> PaymentService::createPayment(const std::string& userId, const std::string& orderId) {
    CoroMapper<Orders> orders(dbClient_);
    std::optional<Orders> order;
    try {
        order = co_await orders.findByPrimaryKey(orderId);
    } catch (const UnexpectedRows&) {
    }
    if (!order || order->getValueOfUserId() != userId) throw NotFound("Order not found");

    CoroMapper<Payments> existing(dbClient_);
    if (co_await existing.count(Criteria(Payments::Cols::_order_id, CompareOperator::EQ, orderId)) > 0) {
        throw Conflict("Order is already paid");
    }

    auto tx = co_await dbClient_->newTransactionCoro();
    try {
        Payments payment;
        payment.setUserId(userId);
        payment.setOrderId(orderId);
        payment.setAmount(order->getValueOfTotalPrice());

        CoroMapper<Payments> payments(tx);
        auto saved = co_await payments.insert(payment);

        co_await tx->execSqlCoro(
            "INSERT INTO user_books (user_id, book_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
            userId, order->getValueOfBookId());

        co_return saved;
    } catch (...) {
        tx->rollback();
        throw;
    }
}
