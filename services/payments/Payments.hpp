#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "events/EventBus.hpp"
#include "models/Payments.h"
#include <string>
#include <vector>

class PaymentService {
    public:
       PaymentService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       drogon::Task<std::vector<drogon_model::bookstore::Payments>> getPaymentsByOrder(const std::string& orderId);

       drogon::Task<std::vector<drogon_model::bookstore::Payments>> getPaymentsByUser(const std::string& userId);

       drogon::Task<drogon_model::bookstore::Payments> getPaymentById(const std::string& id);

       drogon::Task<drogon_model::bookstore::Payments> createPayment(const std::string& userId, const std::string& orderId);   // amount = the order's total_price

    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
