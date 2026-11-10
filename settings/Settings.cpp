#include "Settings.h"
#include "../utils/dotenv.hpp"
#include <memory>
#include <stdexcept>

namespace {
    std::unique_ptr<Settings> g_settings;  

    int toInt(const std::string& key, const std::string& fallback) {
        const std::string raw = dotenv::get_or(key, fallback);
        try { return std::stoi(raw); }
        catch (...) { throw std::runtime_error(key + " must be a number, got '" + raw + "'"); }
    }
}

Settings* getSettings() {
    if (g_settings) return g_settings.get();        

    dotenv::load_dotenv();                               
    auto s = std::make_unique<Settings>();
    s->port        = toInt("PORT", "10000");
    s->threads     = toInt("THREADS", "4");
    s->environment = dotenv::get_or("APP_ENV", "development");
    s->host        = dotenv::get_or("APP_HOST", "127.0.0.1");
    s->db.host        = dotenv::require("DB_HOST");
    s->db.port        = toInt("DB_PORT", "5432");
    s->db.name        = dotenv::require("DB_NAME");
    s->db.user        = dotenv::require("DB_USER");
    s->db.password    = dotenv::require("DB_PASSWORD");
    s->db.schema      = dotenv::get_or("DB_SCHEMA", "public");
    s->db.connections = toInt("DB_CONNECTIONS", "4");
    s->jwt.accessSecret  = dotenv::require("JWT_ACCESS_SECRET");
    s->jwt.refreshSecret = dotenv::require("JWT_REFRESH_SECRET");
    s->jwt.accessExpiration  = dotenv::require("JWT_ACCESS_EXPIRATION");
    s->jwt.refreshExpiration = dotenv::require("JWT_REFRESH_EXPIRATION");
    s->redis.host        = dotenv::get_or("REDIS_HOST", "127.0.0.1");
    s->redis.port        = toInt("REDIS_PORT", "6379");
    s->redis.password    = dotenv::get_or("REDIS_PASSWORD", "");
    s->redis.maxAttempts = toInt("QUEUE_MAX_ATTEMPTS", "3");
    s->redis.maxInFlight = toInt("QUEUE_MAX_IN_FLIGHT", "10");
    s->email.resendApiKey = dotenv::require("RESEND_API_KEY");
    s->email.from         = dotenv::require("EMAIL_FROM");
    s->email.templatesDir = dotenv::get_or("EMAIL_TEMPLATES_DIR", "templates/emails");
    s->email.appUrl       = dotenv::get_or("APP_URL", "http://localhost:3000");
    s->email.assetsUrl    = dotenv::get_or("ASSETS_URL", "");
    s->cloudinary.cloudName = dotenv::require("CLOUDINARY_CLOUD_NAME");
    s->cloudinary.apiKey    = dotenv::require("CLOUDINARY_API_KEY");
    s->cloudinary.apiSecret = dotenv::require("CLOUDINARY_API_SECRET");

    if (s->port < 1 || s->port > 65535) throw std::runtime_error("PORT out of range");

    g_settings = std::move(s);
    return g_settings.get();
}