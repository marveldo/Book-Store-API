#include "Orders.hpp"
#include <drogon/orm/CoroMapper.h>
#include "models/Books.h"
#include "models/Payments.h"
#include "utils/Errors.hpp"

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

Task<vector<Orders>> OrderService::getOrdersByUser(const std::string& userId) {
    CoroMapper<Orders> orders(dbClient_);
    co_return co_await orders.findBy(Criteria(Orders::Cols::_user_id, CompareOperator::EQ, userId));
}

Task<Orders> OrderService::getOrderById(const std::string& id) {
    CoroMapper<Orders> orders(dbClient_);
    try {
        co_return co_await orders.findByPrimaryKey(id);
    } catch (const UnexpectedRows&) {
        throw NotFound("Order not found");
    }
}

Task<Orders> OrderService::createOrder(const std::string& userId, const CreateOrderInput& input) {
    CoroMapper<Books> books(dbClient_);
    int64_t price = 0;
    try {
        price = (co_await books.findByPrimaryKey(input.bookId)).getValueOfPrice();
    } catch (const UnexpectedRows&) {
        throw NotFound("Book not found");
    }

    Orders order;
    order.setUserId(userId);
    order.setBookId(input.bookId);
    order.setQuantity(static_cast<int32_t>(input.quantity));
    order.setTotalPrice(price * input.quantity);

    CoroMapper<Orders> orders(dbClient_);
    co_return co_await orders.insert(order);
}

Task<bool> OrderService::deleteOrder(const std::string& id) {
    CoroMapper<Payments> payments(dbClient_);
    if (co_await payments.count(Criteria(Payments::Cols::_order_id, CompareOperator::EQ, id)) > 0) {
        throw Conflict("Paid orders can't be deleted");
    }

    CoroMapper<Orders> orders(dbClient_);
    co_return (co_await orders.deleteByPrimaryKey(id)) > 0;
}
