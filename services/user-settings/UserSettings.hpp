#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "models/UserSettings.h"
#include "input/UserSettingsInputs.hpp"
#include <string>

class UserSettingsService {
    public:
       explicit UserSettingsService(drogon::orm::DbClientPtr dbClient) : dbClient_(std::move(dbClient)) {}

       drogon::Task<drogon_model::bookstore::UserSettings> getSettingsByUserId(const std::string& userId);

       // called once when a user signs up; the table's defaults fill in theme, language, etc.
       drogon::Task<drogon_model::bookstore::UserSettings> createDefaultSettings(const std::string& userId);

       drogon::Task<drogon_model::bookstore::UserSettings> updateSettings(const std::string& userId, const UpdateSettingsInput& input);

    private:
       drogon::orm::DbClientPtr dbClient_;
};
