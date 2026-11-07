#pragma once 
#include <string>
#include <optional>
#include <memory> 
#include "utils/Role.hpp"

namespace {
     auto conversion = [](const std::string& str) -> std::chrono::seconds {
        if (str.empty()) return std::chrono::seconds(0);
        char unit = str.back();
        int value = std::stoi(str.substr(0, str.size() - 1));
        switch (unit) {
            case 's': return std::chrono::seconds(value);
            case 'm': return std::chrono::minutes(value);
            case 'h': return std::chrono::hours(value);
            case 'd': return std::chrono::hours(value * 24);
            default: throw std::invalid_argument("Invalid time unit in expiration string");
      }};
}
struct Claims {
    std::string userId;
    std::string tokenType;
    std::string email;      
    Role role = Role::User;
};

class JwtService {
    public:
        explicit JwtService(const std::string& accessSecret, const std::string& refreshSecret, std::string accessExpiration, std::string refreshExpiration)
            : accessSecret_(accessSecret), refreshSecret_(refreshSecret) , accessExpiration_(accessExpiration), refreshExpiration_(refreshExpiration) {}

        std::string generateAccessToken(Claims& claims);
        std::string generateRefreshToken(Claims& claims);

        std::shared_ptr<Claims> verifyAccessToken(const std::string& token);
        std::shared_ptr<Claims> verifyRefreshToken(const std::string& token);

    private:
        std::string accessSecret_;
        std::string refreshSecret_;
        std::string accessExpiration_;
        std::string refreshExpiration_;
};

