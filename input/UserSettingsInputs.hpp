#pragma once
#include "utils/Validation.hpp"
#include <optional>
#include <string>
#include <string_view>

enum class Theme { Light, Dark, System };

// The database stores the theme as text ('light' / 'dark' / 'system').
inline const char* toString(Theme t) {
    switch (t) {
        case Theme::Light:  return "light";
        case Theme::Dark:   return "dark";
        case Theme::System: return "system";
    }
    return "light";
}

struct UpdateSettingsInput {                     // every field optional: nullopt = leave unchanged
    std::optional<Theme>       theme;
    std::optional<std::string> language;
    std::optional<std::string> timeZone;
    std::optional<bool>        emailNotifications;
};

inline ParseResult<UpdateSettingsInput> parseUpdateSettings(std::string_view raw) {
    return parseInput<UpdateSettingsInput>(raw, [](Validator& v) {
        v.onlyAllow({"theme", "language", "time_zone", "email_notifications"});
        UpdateSettingsInput in{
            v.optionalEnum<Theme>("theme", {{"light", Theme::Light}, {"dark", Theme::Dark}, {"system", Theme::System}}),
            v.optionalString("language", 2, 10),
            v.optionalString("time_zone", 1, 64),
            v.optionalBool("email_notifications"),
        };
        if (v.ok() && !in.theme && !in.language && !in.timeZone && !in.emailNotifications)
            v.addError("body", "must contain at least one setting to update");
        return in;
    });
}
