#include "PaymentsController.h"
#include "utils/MoneyJson.hpp"
#include "utils/Errors.hpp"
#include <nlohmann/json.hpp>

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

Task<HttpResponsePtr> PaymentsController::Verify(HttpRequestPtr req, std::string reference) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto& payments = *services_->paymentService;

    const auto payment = co_await payments.getPaymentByReference(reference);
    if (payment.getValueOfUserId() != userId) throw ::NotFound("Payment not found");

    try {
        co_await payments.settle(reference, SettleBy::Request);
    } catch (const AppError&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN << "verify " << reference << ": " << e.what();
        co_return res::error(k502BadGateway, "Couldn't confirm the payment with Paystack right now. Try again shortly.");
    }

    co_return res::success(paymentJson(co_await payments.getPaymentByReference(reference)));
}

Task<HttpResponsePtr> PaymentsController::Webhook(HttpRequestPtr req) {
    auto& payments = *services_->paymentService;
    if (!payments.validWebhook(req->body(), req->getHeader("x-paystack-signature"))) {
        co_return res::error(k401Unauthorized, "Bad signature");
    }

    const auto event = nlohmann::json::parse(req->body(), nullptr, false);
    if (!event.is_object() || !event.contains("data") || !event["data"].is_object()) {
        co_return res::badRequest("Bad webhook body");
    }

    const auto type = event.value("event", "");
    const auto& data = event["data"];

    if (type == "charge.success")
        payments.queueVerify(data.value("reference", ""));
    else if (type == "refund.processed")
        co_await payments.refundFinished(data.value("transaction_reference", ""), true);
    else if (type == "refund.failed")
        co_await payments.refundFinished(data.value("transaction_reference", ""), false);

    co_return HttpResponse::newHttpResponse();
}
