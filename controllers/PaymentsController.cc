#include "PaymentsController.h"
#include "utils/MoneyJson.hpp"
#include "utils/Errors.hpp"

Task<HttpResponsePtr> PaymentsController::MyPayments(HttpRequestPtr req) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto payments = co_await services_->paymentService->getPaymentsByUser(userId);
    co_return res::success(res::toJsonArray(payments, paymentJson));
}

Task<HttpResponsePtr> PaymentsController::GetPayment(HttpRequestPtr req, std::string id) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto payment = co_await services_->paymentService->getPaymentById(id);
    if (payment.getValueOfUserId() != userId) throw ::NotFound("Payment not found");
    co_return res::success(paymentJson(payment));
}
