#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "events/EventBus.hpp"
#include "models/UserSettings.h"
#include "input/UserSettingsInputs.hpp"
#include <string>

class UserSettingsService {
    public:
       UserSettingsService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {}

       drogon::Task<drogon_model::bookstore::UserSettings> getSettingsByUserId(const std::string& userId);

       // called once when a user signs up; the table's defaults fill in theme, language, etc.
       drogon::Task<drogon_model::bookstore::UserSettings> createDefaultSettings(const std::string& userId);

       drogon::Task<drogon_model::bookstore::UserSettings> updateSettings(const std::string& userId, const UpdateSettingsInput& input);

    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};
