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
    ADD_METHOD_TO(AuthController::Login, API_V1 + "/auth/login", Post, "LoginRateLimit");
    ADD_METHOD_TO(AuthController::Refresh, API_V1 + "/auth/refresh", Post, "RefreshRateLimit");
    METHOD_LIST_END

    Task<HttpResponsePtr> Login(HttpRequestPtr req);
    // A new access token (and a new refresh token) from a refresh token, without the password.
    Task<HttpResponsePtr> Refresh(HttpRequestPtr req);

  private:
    // The login/refresh response: both tokens plus the user.
    Json::Value tokensFor(const drogon_model::bookstore::Users& user);

    Services* services_;
};
