#include "Payments.hpp"
#include <drogon/orm/CoroMapper.h>
#include <drogon/utils/Utilities.h>
#include "events/Events.hpp"
#include "models/Orders.h"
#include "models/Users.h"
#include <algorithm>
#include <optional>
#include <unordered_map>
#include "utils/Errors.hpp"
#include "utils/DbTransaction.hpp"

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

namespace {
    enum class RemoteStatus { Success, Failed, Pending, Unknown };

    RemoteStatus toRemoteStatus(const std::string& status) {
        static const std::unordered_map<std::string, RemoteStatus> statuses = {
            {"success", RemoteStatus::Success},
            {"failed", RemoteStatus::Failed},
            {"reversed", RemoteStatus::Failed},
            {"abandoned", RemoteStatus::Pending},
            {"ongoing", RemoteStatus::Pending},
            {"pending", RemoteStatus::Pending},
            {"processing", RemoteStatus::Pending},
            {"queued", RemoteStatus::Pending},
        };
        const auto it = statuses.find(status);
        return it == statuses.end() ? RemoteStatus::Unknown : it->second;
    }

    Criteria byReference(const std::string& reference) {
        return Criteria(Payments::Cols::_reference, CompareOperator::EQ, reference);
    }

    Criteria withStatus(const std::string& status) {
        return Criteria(Payments::Cols::_status, CompareOperator::EQ, status);
    }

    const std::vector<std::string>& statusColumn() {
        static const std::vector<std::string> columns{Payments::Cols::_status};
        return columns;
    }
}

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

Task<Payments> PaymentService::getPaymentByReference(const std::string& reference) {
    CoroMapper<Payments> payments(dbClient_);
    try {
        co_return co_await payments.findOne(byReference(reference));
    } catch (const UnexpectedRows&) {
        throw NotFound("Payment not found");
    }
}

Task<Checkout> PaymentService::startCheckout(const std::string& userId, const std::string& orderId, const std::string& callbackUrl) {
    CoroMapper<Orders> orders(dbClient_);
    std::optional<Orders> order;
    try {
        order = co_await orders.findByPrimaryKey(orderId);
    } catch (const UnexpectedRows&) {
    }
    if (!order || order->getValueOfUserId() != userId) throw NotFound("Order not found");

    CoroMapper<Payments> payments(dbClient_);
    const Criteria forOrder(Payments::Cols::_order_id, CompareOperator::EQ, orderId);

    if (co_await payments.count(forOrder && withStatus("success")) > 0) {
        throw Conflict("Order is already paid");
    }

    auto open = co_await payments.findBy(forOrder && withStatus("pending"));
    if (!open.empty()) {
        co_return Checkout{open.front().getValueOfReference(), open.front().getValueOfCheckoutUrl()};
    }

    CoroMapper<Users> users(dbClient_);
    const auto user = co_await users.findByPrimaryKey(userId);
    const auto reference = "mb_" + drogon::utils::getUuid();
    const auto checkout = co_await paystack_->initialize(user.getValueOfEmail(), order->getValueOfTotalPrice(), reference, callbackUrl);

    Payments payment;
    payment.setUserId(userId);
    payment.setOrderId(orderId);
    payment.setAmount(order->getValueOfTotalPrice());
    payment.setCurrency("NGN");
    payment.setReference(reference);
    payment.setCheckoutUrl(checkout.authorizationUrl);
    payment.setStatus("pending");
    payment.setNextCheckAt((co_await databaseNow(dbClient_)).after(FIRST_CHECK_SECONDS));
    try {
        co_await payments.insert(payment);
    } catch (const ForeignKeyViolation&) {
        throw NotFound("Order not found");
    }

    co_return Checkout{reference, checkout.authorizationUrl};
}

Task<std::string> PaymentService::settle(const std::string& reference, SettleBy by) {
    const auto current = co_await getPaymentByReference(reference);
    if (isFinal(current.getValueOfStatus())) co_return current.getValueOfStatus();

    const auto remote = co_await paystack_->verify(reference);

    std::string status;
    bool refundDue = false;

    auto tx = co_await dbClient_->newTransactionCoro();
    try {
        CoroMapper<Payments> payments(tx);
        auto payment = co_await payments.forUpdate().findOne(byReference(reference));
        if (isFinal(payment.getValueOfStatus())) co_return payment.getValueOfStatus();

        const auto now = co_await databaseNow(tx);
        const bool wasAbandoned = payment.getValueOfStatus() == "abandoned";

        const auto amountMatches = [&] {
            return remote.amount == payment.getValueOfAmount() && remote.currency == payment.getValueOfCurrency();
        };

        const auto orderAlreadyPaid = [&]() -> Task<bool> {
            co_return co_await payments.count(
                Criteria(Payments::Cols::_order_id, CompareOperator::EQ, payment.getValueOfOrderId()) &&
                withStatus("success")) > 0;
        };

        const auto markPaid = [&]() -> Task<> {
            co_await grantBook(tx, payment);
            payment.setPaidAt(now);
            close(payment, "success");
        };

        const auto owesRefund = [&](const char* reason) {
            LOG_WARN << "Payment " << reference << " will be refunded: " << reason;
            payment.setPaidAt(now);
            close(payment, "refund_pending");
            refundDue = true;
        };

        const auto rejectAmount = [&] {
            LOG_ERROR << "Payment " << reference << " paid " << remote.amount << " " << remote.currency
                      << ", expected " << payment.getValueOfAmount() << " " << payment.getValueOfCurrency();
            owesRefund("wrong amount");
        };

        const auto markFailed = [&] {
            close(payment, "failed");
        };

        const auto checkLater = [&] {
            if (by == SettleBy::Poller && !wasAbandoned) scheduleNextCheck(payment, now);
        };

        const auto reportUnknown = [&] {
            LOG_WARN << "Payment " << reference << ": unknown Paystack status '" << remote.status << "'";
            checkLater();
        };

        const auto handleSuccess = [&]() -> Task<> {
            if (!amountMatches()) rejectAmount();
            else if (wasAbandoned) owesRefund("paid after the checkout was abandoned");
            else if (co_await orderAlreadyPaid()) owesRefund("order was already paid");
            else co_await markPaid();
        };

        switch (toRemoteStatus(remote.status)) {
            case RemoteStatus::Success:
                co_await handleSuccess();
                break;
            case RemoteStatus::Failed:
                markFailed();
                break;
            case RemoteStatus::Pending:
                checkLater();
                break;
            case RemoteStatus::Unknown:
                reportUnknown();
                break;
        }

        co_await payments.update(payment);
        status = payment.getValueOfStatus();
    } catch (...) {
        tx->rollback();
        throw;
    }

    if (!co_await CommitAwaiter(std::move(tx))) throw std::runtime_error("Payment " + reference + ": commit failed");
    if (refundDue) events_->Publish(PAYMENT_REFUND_DUE, PaymentRefundDue{reference});
    co_return status;
}

Task<vector<std::string>> PaymentService::claimDue(int limit) {
    const auto rows = co_await dbClient_->execSqlCoro(
        "UPDATE payments SET next_check_at = now() + interval '2 minutes' "
        "WHERE id IN (SELECT id FROM payments "
        "             WHERE status = 'pending' AND next_check_at <= now() "
        "             ORDER BY next_check_at LIMIT $1 FOR UPDATE SKIP LOCKED) "
        "RETURNING reference", static_cast<int64_t>(limit));

    vector<std::string> references;
    references.reserve(rows.size());
    for (const auto& row : rows) references.push_back(row["reference"].as<std::string>());
    co_return references;
}

Task<> PaymentService::recordFailedCheck(const std::string& reference) {
    auto tx = co_await dbClient_->newTransactionCoro();
    try {
        CoroMapper<Payments> payments(tx);
        auto payment = co_await payments.forUpdate().findOne(byReference(reference));
        if (payment.getValueOfStatus() != "pending") co_return;

        scheduleNextCheck(payment, co_await databaseNow(tx));
        co_await payments.update(payment);
    } catch (...) {
        tx->rollback();
        throw;
    }
}

Task<> PaymentService::refund(const std::string& reference) {
    CoroMapper<Payments> payments(dbClient_);

    const auto payment = co_await payments.findOne(byReference(reference));
    if (payment.getValueOfStatus() != "refund_pending") co_return;

    co_await paystack_->refund(reference);

    co_await payments.updateBy(statusColumn(),
                               byReference(reference) && withStatus("refund_pending"),
                               "refunding");
}

Task<> PaymentService::refundFinished(const std::string& reference, bool processed) {
    CoroMapper<Payments> payments(dbClient_);
    co_await payments.updateBy(statusColumn(),
                               byReference(reference) && withStatus("refunding"),
                               processed ? "refunded" : "refund_failed");
    if (!processed) LOG_ERROR << "Refund for " << reference << " failed at Paystack: refund it by hand";
}

void PaymentService::queueVerify(const std::string& reference) {
    events_->Publish(PAYMENT_VERIFY_DUE, PaymentVerifyDue{reference});
}

bool PaymentService::validWebhook(std::string_view body, std::string_view signature) const {
    return paystack_->validSignature(body, signature);
}

bool PaymentService::isFinal(const std::string& status) {
    return status != "pending" && status != "abandoned";
}

void PaymentService::close(Payments& payment, const std::string& status) {
    payment.setStatus(status);
    payment.setNextCheckAtToNull();
}

Task<trantor::Date> PaymentService::databaseNow(const DbClientPtr& client) {
    const auto rows = co_await client->execSqlCoro("SELECT now()::timestamp AS now");
    co_return trantor::Date::fromDbStringLocal(rows[0]["now"].as<std::string>());
}

void PaymentService::scheduleNextCheck(Payments& payment, const trantor::Date& now) const {
    const int tries = payment.getValueOfCheckCount() + 1;
    payment.setCheckCount(tries);

    if (tries >= maxChecks_) {
        close(payment, "abandoned");
        return;
    }

    const int64_t wait = std::min<int64_t>(MAX_WAIT_SECONDS, BASE_WAIT_SECONDS << std::min(tries - 1, 4));
    payment.setNextCheckAt(now.after(static_cast<double>(wait)));
}

Task<> PaymentService::grantBook(const std::shared_ptr<Transaction>& tx, const Payments& payment) {
    CoroMapper<Orders> orders(tx);
    const auto order = co_await orders.findByPrimaryKey(payment.getValueOfOrderId());
    co_await tx->execSqlCoro(
        "INSERT INTO user_books (user_id, book_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
        payment.getValueOfUserId(), order.getValueOfBookId());
}
