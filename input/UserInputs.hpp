#pragma once
#include "utils/Validation.hpp"
#include <cctype>
#include <optional>
#include <algorithm>
#include <regex>
#include <string>
#include <string_view>

namespace {
    std::string toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return str;
}
}

struct CreateUserInput {
    std::string username;
    std::string email;
    std::string password;              // plain text from the client: hash it before saving
};

struct LoginUserInput {
    std::string email;
    std::string password;
};

struct UpdateUserInput {
    std::optional<std::string> username;   // nullopt = leave unchanged
    std::optional<std::string> email;
};

struct ChangePasswordInput {
    std::string currentPassword;
    std::string newPassword;
};

namespace user_rules {
    inline const std::regex username(R"([A-Za-z0-9_]+)");
    inline const std::regex email(R"([^@\s]+@[^@\s]+\.[^@\s]+)");
}


inline void checkPasswordStrength(Validator& v, const std::optional<std::string>& password, const std::string& field) {
    if (!password) return;                      // already failed "required" / length checks
    bool upper = false, lower = false, special = false;
    for (unsigned char c : *password) {
        if (std::isupper(c))      upper = true;
        else if (std::islower(c)) lower = true;
        else if (!std::isdigit(c)) special = true;   // anything that isn't a letter or digit: !, -, space, etc.
    }
    if (!upper)   v.addError(field, "must contain at least one uppercase letter");
    if (!lower)   v.addError(field, "must contain at least one lowercase letter");
    if (!special) v.addError(field, "must contain at least one special character");
}

inline ParseResult<CreateUserInput> parseCreateUser(std::string_view raw) {
    return parseInput<CreateUserInput>(raw, [](Validator& v) {
        v.onlyAllow({"username", "email", "password"});
        auto username = v.requiredString("username", 3, 50);
        auto email    = v.requiredString("email", 3, 255);
        auto password = v.requiredString("password", 8, 128);
        if (email) email = toLower(*email);
        v.matches(username, "username", user_rules::username, "may only contain letters, numbers and _");
        v.matches(email,    "email",    user_rules::email,    "must be a valid email address");
        checkPasswordStrength(v, password, "password");
        return CreateUserInput{username.value_or(""), email.value_or(""), password.value_or("")};
    });
}

inline ParseResult<UpdateUserInput> parseUpdateUser(std::string_view raw) {
    return parseInput<UpdateUserInput>(raw, [](Validator& v) {
        v.onlyAllow({"username", "email"});
        auto username = v.optionalString("username", 3, 50);
        auto email    = v.optionalString("email", 3, 255);
        if (email) email = toLower(*email);
        v.matches(username, "username", user_rules::username, "may only contain letters, numbers and _");
        v.matches(email,    "email",    user_rules::email,    "must be a valid email address");
        if (v.ok() && !username && !email) v.addError("body", "must contain at least one of: username, email");
        return UpdateUserInput{username, email};
    });
}

inline ParseResult<ChangePasswordInput> parseChangePassword(std::string_view raw) {
    return parseInput<ChangePasswordInput>(raw, [](Validator& v) {
        v.onlyAllow({"current_password", "new_password"});
        auto current = v.requiredString("current_password", 1, 128);
        auto next    = v.requiredString("new_password", 8, 128);
        checkPasswordStrength(v, next, "new_password");
        if (current && next && *current == *next) v.addError("new_password", "must be different from the current password");
        return ChangePasswordInput{current.value_or(""), next.value_or("")};
    });
}

inline ParseResult<LoginUserInput> parseLoginCredentials(std::string_view raw) {
    return parseInput<LoginUserInput>(raw , [](Validator& v){
        v.onlyAllow({"email", "password"});
        auto email    = v.requiredString("email", 3, 255);
        auto password = v.requiredString("password", 8, 128);
        if (email) email = toLower(*email);
        v.matches(email, "email", user_rules::email,"must be a valid email address");
        return LoginUserInput{ email.value_or(""), password.value_or("")};
    });
}

