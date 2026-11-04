#pragma once
#include <optional>
#include <string>

enum class Role { User, Admin };

inline std::string toString(Role role) {
    return role == Role::Admin ? "ADMIN" : "USER";
}

inline std::optional<Role> roleFromString(const std::string& value) {
    if (value == "USER")  return Role::User;
    if (value == "ADMIN") return Role::Admin;
    return std::nullopt;
}
