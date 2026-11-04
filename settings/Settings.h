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

struct Settings {
    int              port;
    int              threads;
    std::string      host;
    std::string      environment; 
    DatabaseSettings db;
    JWTConfig        jwt;

    bool isProduction() const { return environment == "production"; }
};


Settings* getSettings();