#pragma once
#include <string>
#include <drogon/orm/DbClient.h>

struct DatabaseSettings {
    std::string url;            // DB_URL (e.g. on Render). When set, the fields below are ignored.
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
    std::string username;       // Redis 6+ ACL user; empty = the "default" user
    int         db = 0;         // database number (SELECT)
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
    std::string apiUrl;          // https://api.cloudinary.com unless overridden (e.g. a mock in tests)
};

struct UploadSettings {
    size_t pdfMaxBytes;
    int    pdfMaxPages;
    int    pdfTimeoutSeconds;
    int    pdfMaxImages;
    int    pdfMaxConcurrent;   // PDF imports running at once on each server
};

struct PaymentSettings {
    std::string paystackSecretKey;
    std::string paystackApiUrl;        // https://api.paystack.co unless overridden (e.g. a mock in tests)
    int         maxChecks;             // poller checks of a pending payment before it is abandoned
};

struct CronSettings {
    std::string secret;                // X-Cron-Secret for POST /internal/cron/{task}; empty turns the route off
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
    UploadSettings   uploads;
    PaymentSettings  payments;
    CronSettings     cron;

    bool isProduction() const { return environment == "production"; }
};


Settings* getSettings();