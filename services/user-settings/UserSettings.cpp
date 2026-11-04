#include "UserSettings.hpp"
#include <drogon/orm/CoroMapper.h>

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;

Task<UserSettings> UserSettingsService::getSettingsByUserId(const std::string& userId) {
    CoroMapper<UserSettings> settings(dbClient_);
    try {
        co_return co_await settings.findOne(Criteria(UserSettings::Cols::_user_id, CompareOperator::EQ, userId));
    } catch (const UnexpectedRows&) {
    }
    co_return co_await createDefaultSettings(userId);
}

Task<UserSettings> UserSettingsService::createDefaultSettings(const std::string& userId) {
    CoroMapper<UserSettings> settings(dbClient_);
    UserSettings row;
    row.setUserId(userId);
    co_return co_await settings.insert(row);
}

Task<UserSettings> UserSettingsService::updateSettings(const std::string& userId, const UpdateSettingsInput& input) {
    CoroMapper<UserSettings> settings(dbClient_);
    auto row = co_await getSettingsByUserId(userId);

    if (input.theme)              row.setTheme(toString(*input.theme));
    if (input.language)           row.setLanguage(*input.language);
    if (input.timeZone)           row.setTimeZone(*input.timeZone);
    if (input.emailNotifications) row.setEmailNotifications(*input.emailNotifications);

    co_await settings.update(row);
    co_return co_await getSettingsByUserId(userId);
}
