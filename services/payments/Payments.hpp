#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "events/EventBus.hpp"
#include "models/Payments.h"
#include "PaystackClient.hpp"
#include <string>
#include <vector>

enum class SettleBy { Poller, Request };

struct Checkout {
    std::string reference;
    std::string authorizationUrl;
};

class PaymentService {
    public:
       PaymentService(drogon::orm::DbClientPtr dbClient, EventBus* events, PaystackClient* paystack, int maxChecks)
           : dbClient_(std::move(dbClient)), events_(events), paystack_(paystack), maxChecks_(maxChecks) {}

       drogon::Task<std::vector<drogon_model::bookstore::Payments>> getPaymentsByOrder(const std::string& orderId);

       drogon::Task<std::vector<drogon_model::bookstore::Payments>> getPaymentsByUser(const std::string& userId);

       drogon::Task<drogon_model::bookstore::Payments> getPaymentById(const std::string& id);

       drogon::Task<drogon_model::bookstore::Payments> getPaymentByReference(const std::string& reference);

       drogon::Task<Checkout> startCheckout(const std::string& userId, const std::string& orderId, const std::string& callbackUrl);

       drogon::Task<std::string> settle(const std::string& reference, SettleBy by);

       drogon::Task<std::vector<std::string>> claimDue(int limit);

       drogon::Task<> recordFailedCheck(const std::string& reference);

       drogon::Task<> refund(const std::string& reference);

       drogon::Task<> refundFinished(const std::string& reference, bool processed);

       void queueVerify(const std::string& reference);

       bool validWebhook(std::string_view body, std::string_view signature) const;

    private:
       static bool isFinal(const std::string& status);
       static void close(drogon_model::bookstore::Payments& payment, const std::string& status);
       void scheduleNextCheck(drogon_model::bookstore::Payments& payment, const trantor::Date& now) const;
       static drogon::Task<trantor::Date> databaseNow(const drogon::orm::DbClientPtr& client);
       drogon::Task<> grantBook(const std::shared_ptr<drogon::orm::Transaction>& tx,
                                const drogon_model::bookstore::Payments& payment);

       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
       PaystackClient* paystack_;
       int maxChecks_;

       static constexpr double FIRST_CHECK_SECONDS = 600;
       static constexpr int64_t BASE_WAIT_SECONDS = 300;
       static constexpr int64_t MAX_WAIT_SECONDS = 3600;
};
