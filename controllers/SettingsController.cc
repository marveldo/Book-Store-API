#include "SettingsController.h"
#include "input/UserSettingsInputs.hpp"

Task<HttpResponsePtr> SettingsController::GetSettings(HttpRequestPtr req) {
    const auto userId = req->attributes()->get<std::string>("id");
    auto settings = co_await services_->userSettingsService->getSettingsByUserId(userId);
    co_return res::success(settings.toJson());
}

Task<HttpResponsePtr> SettingsController::UpdateSettings(HttpRequestPtr req) {
    auto parsed = parseUpdateSettings(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) {
        co_return res::badRequest(*errors);
    }

    const auto userId = req->attributes()->get<std::string>("id");
    auto settings = co_await services_->userSettingsService->updateSettings(userId, std::get<UpdateSettingsInput>(parsed));
    co_return res::success(settings.toJson());
}
