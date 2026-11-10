#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "events/EventBus.hpp"
#include "models/Orders.h"
#include "input/OrderInputs.hpp"
#include <string>
#include <vector>

class OrderService {
    public:
       OrderService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       drogon::Task<std::vector<drogon_model::bookstore::Orders>> getOrdersByUser(const std::string& userId);

       drogon::Task<drogon_model::bookstore::Orders> getOrderById(const std::string& id);
       
       drogon::Task<drogon_model::bookstore::Orders> createOrder(const std::string& userId, const CreateOrderInput& input);

       drogon::Task<bool> deleteOrder(const std::string& id);

    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
