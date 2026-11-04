#include "OrdersController.h"
#include "input/OrderInputs.hpp"
#include "utils/Errors.hpp"

Task<drogon_model::bookstore::Orders> OrdersController::ownOrder(const std::string& userId, const std::string& orderId) {
    auto order = co_await services_->orderService->getOrderById(orderId);
    if (order.getValueOfUserId() != userId) throw ::NotFound("Order not found");
    co_return order;
}

Task<HttpResponsePtr> OrdersController::MyOrders(HttpRequestPtr req) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto orders = co_await services_->orderService->getOrdersByUser(userId);
    co_return res::success(res::toJsonArray(orders));
}

Task<HttpResponsePtr> OrdersController::CreateOrder(HttpRequestPtr req) {
    auto parsed = parseCreateOrder(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    const auto userId = req->attributes()->get<std::string>("id");
    auto order = co_await services_->orderService->createOrder(userId, std::get<CreateOrderInput>(parsed));
    co_return res::success(order.toJson(), k201Created);
}

Task<HttpResponsePtr> OrdersController::GetOrder(HttpRequestPtr req, std::string id) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto order = co_await ownOrder(userId, id);
    co_return res::success(order.toJson());
}

Task<HttpResponsePtr> OrdersController::DeleteOrder(HttpRequestPtr req, std::string id) {
    const auto userId = req->attributes()->get<std::string>("id");
    co_await ownOrder(userId, id);
    co_await services_->orderService->deleteOrder(id);
    co_return res::success(Json::Value());
}

Task<HttpResponsePtr> OrdersController::PayOrder(HttpRequestPtr req, std::string id) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto payment = co_await services_->paymentService->createPayment(userId, id);
    co_return res::success(payment.toJson(), k201Created);
}
