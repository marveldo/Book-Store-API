#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class OrdersController : public drogon::HttpController<OrdersController, false>
{
  public:
    explicit OrdersController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(OrdersController::MyOrders, API_V1 + "/orders", Get, "JwtMiddleWare");
    ADD_METHOD_TO(OrdersController::CreateOrder, API_V1 + "/orders", Post, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(OrdersController::GetOrder, API_V1 + "/orders/([0-9a-fA-F-]{36})", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(OrdersController::DeleteOrder, API_V1 + "/orders/([0-9a-fA-F-]{36})", Delete, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(OrdersController::PayOrder, API_V1 + "/orders/([0-9a-fA-F-]{36})/pay", Post, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> MyOrders(HttpRequestPtr req);
    Task<HttpResponsePtr> CreateOrder(HttpRequestPtr req);
    Task<HttpResponsePtr> GetOrder(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> DeleteOrder(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> PayOrder(HttpRequestPtr req, std::string id);

  private:
    Task<drogon_model::bookstore::Orders> ownOrder(const std::string& userId, const std::string& orderId);

    Services* services_;
};
