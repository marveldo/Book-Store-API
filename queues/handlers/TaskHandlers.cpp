#include "TaskHandlers.hpp"
#include "TaskMiddlewares.hpp"
#include <drogon/orm/Exception.h>

void registerTaskHandlers(TaskQueue* queue, EmailService* email, Services* services) {
    queue->registerHandler("send_email", [email](const nlohmann::json& payload) -> drogon::Task<> {
        co_await email->send(payload.at("to").get<std::string>(),
                             payload.at("subject").get<std::string>(),
                             payload.at("template").get<std::string>(),
                             payload.value("data", nlohmann::json::object()));
    }, {timed("send_email")});

    queue->registerHandler("create_user_settings", [services](const nlohmann::json& payload) -> drogon::Task<> {
        try {
            co_await services->userSettingsService->createDefaultSettings(payload.at("user_id").get<std::string>());
        } catch (const drogon::orm::UniqueViolation&) {
        }
    },{timed("create_user_settings")});
}
