#pragma once
#include <string>

inline const std::string USER_CREATED = "user.created";

struct UserCreated {
    std::string id;
    std::string email;
    std::string username;
};
