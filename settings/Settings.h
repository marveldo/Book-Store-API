#pragma once
#include <string>
#include <drogon/orm/DbClient.h>

struct DatabaseSettings {
    std::string host;
    int         port;
    std::string name;
    std::string user;
    std::string password;
    std::string schema;
    int         connections;
};

struct JWTConfig {
    std::string accessSecret;
    std::string refreshSecret;
    std::string accessExpiration;
    std::string refreshExpiration;
};

struct RedisSettings {
    std::string host;
    int         port;
    std::string password;
    int         maxAttempts;
    int         maxInFlight;
};

struct EmailSettings {
    std::string resendApiKey;
    std::string from;
    std::string templatesDir;
    std::string appUrl;
    std::string assetsUrl;
};

struct CloudinarySettings {
    std::string cloudName;
    std::string apiKey;
    std::string apiSecret;
};

struct Settings {
    int              port;
    int              threads;
    std::string      host;
    std::string      environment; 
    DatabaseSettings db;
    JWTConfig        jwt;
    RedisSettings    redis;
    EmailSettings    email;
    CloudinarySettings cloudinary;

    bool isProduction() const { return environment == "production"; }
};


Settings* getSettings();