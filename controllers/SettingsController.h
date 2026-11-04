#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class SettingsController : public drogon::HttpController<SettingsController, false>
{
  public:
    explicit SettingsController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SettingsController::GetSettings, API_V1 + "/me/settings", Get, "JwtMiddleWare");
    ADD_METHOD_TO(SettingsController::UpdateSettings, API_V1 + "/me/settings", Put, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> GetSettings(HttpRequestPtr req);
    Task<HttpResponsePtr> UpdateSettings(HttpRequestPtr req);

  private:
    Services* services_;
};
