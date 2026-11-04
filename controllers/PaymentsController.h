#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class PaymentsController : public drogon::HttpController<PaymentsController, false>
{
  public:
    explicit PaymentsController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PaymentsController::MyPayments, API_V1 + "/payments", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(PaymentsController::GetPayment, API_V1 + "/payments/([0-9a-fA-F-]{36})", Get, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> MyPayments(HttpRequestPtr req);
    Task<HttpResponsePtr> GetPayment(HttpRequestPtr req, std::string id);

  private:
    Services* services_;
};
