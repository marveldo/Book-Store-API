#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "models/Orders.h"
#include "input/OrderInputs.hpp"
#include <string>
#include <vector>

class OrderService {
    public:
       explicit OrderService(drogon::orm::DbClientPtr dbClient) : dbClient_(std::move(dbClient)) {}

       drogon::Task<std::vector<drogon_model::bookstore::Orders>> getOrdersByUser(const std::string& userId);

       drogon::Task<drogon_model::bookstore::Orders> getOrderById(const std::string& id);
       
       drogon::Task<drogon_model::bookstore::Orders> createOrder(const std::string& userId, const CreateOrderInput& input);

       drogon::Task<bool> deleteOrder(const std::string& id);

    private:
       drogon::orm::DbClientPtr dbClient_;
};
