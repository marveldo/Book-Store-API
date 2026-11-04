#pragma once 
#include <string>
#include <optional>
#include <memory> 

struct Claims {
    std::string userId;
    std::string tokenType;
    std::string email;      
};

class JwtService {
    public:
        explicit JwtService(const std::string& accessSecret, const std::string& refreshSecret, std::string accessExpiration, std::string refreshExpiration)
            : accessSecret_(accessSecret), refreshSecret_(refreshSecret) , accessExpiration_(accessExpiration), refreshExpiration_(refreshExpiration) {}

        std::string generateAccessToken(Claims& claims);
        std::string generateRefreshToken(Claims& claims);

        std::optional<std::string> verifyAccessToken(const std::string& token);
        std::optional<std::string> verifyRefreshToken(const std::string& token);

        std::shared_ptr<Claims> verifyToken(const std::string& token);

    private:
        std::string accessSecret_;
        std::string refreshSecret_;
        std::string accessExpiration_;
        std::string refreshExpiration_;
};

