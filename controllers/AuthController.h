#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class AuthController : public drogon::HttpController<AuthController, false>
{
  public:

    explicit AuthController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::Login, API_V1 + "/auth/login", Post);
    METHOD_LIST_END

    Task<HttpResponsePtr> Login(HttpRequestPtr req);

  private:
    Services* services_;
};
