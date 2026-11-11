#include "Settings.h"
#include "../utils/dotenv.hpp"
#include <memory>
#include <stdexcept>
#include <cctype>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace {
    std::unique_ptr<Settings> g_settings;  

    int toInt(const std::string& key, const std::string& fallback) {
        const std::string raw = dotenv::get_or(key, fallback);
        try { return std::stoi(raw); }
        catch (...) { throw std::runtime_error(key + " must be a number, got '" + raw + "'"); }
    }

    // Drogon's Redis client (trantor::InetAddress) only accepts an IP like "10.0.0.5".
    // Render gives Key Value a hostname like "red-abc123", so look it up once at startup.
    std::string resolveHost(const std::string& key, const std::string& host) {
        in_addr ip{};
        if (inet_pton(AF_INET, host.c_str(), &ip) == 1) return host;   // already an IP

    #ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    #endif
        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* found = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || !found) {
            throw std::runtime_error(key + ": could not resolve host '" + host + "'");
        }
        char text[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(found->ai_addr)->sin_addr, text, sizeof(text));
        freeaddrinfo(found);
        return text;
    }

    // %40 -> @ etc. Passwords in URLs have their special characters encoded.
    std::string percentDecode(const std::string& in) {
        std::string out;
        for (size_t i = 0; i < in.size(); i++) {
            if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) && std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
                out += static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16));
                i += 2;
            } else {
                out += in[i];
            }
        }
        return out;
    }

    // redis://[user][:password@]host[:port][/db], the form Render's Key Value gives you.
    void applyRedisUrl(const std::string& url, RedisSettings& redis) {
        std::string rest = url;
        const auto scheme = rest.find("://");
        if (scheme == std::string::npos) throw std::runtime_error("REDIS_URL must look like redis://host:port");
        if (rest.substr(0, scheme) == "rediss") {
            // Fail loudly: otherwise the server starts and the queue silently never connects.
            throw std::runtime_error("REDIS_URL uses rediss:// (TLS), which Drogon's Redis client doesn't support. "
                                     "Use a Redis that accepts plain redis:// connections (e.g. Render Key Value, Redis Cloud).");
        }
        if (rest.substr(0, scheme) != "redis") throw std::runtime_error("REDIS_URL must start with redis://");
        rest = rest.substr(scheme + 3);

        if (const auto slash = rest.find('/'); slash != std::string::npos) {
            const std::string db = rest.substr(slash + 1);
            if (!db.empty()) {
                try { redis.db = std::stoi(db); }
                catch (...) { throw std::runtime_error("REDIS_URL: database after '/' must be a number, got '" + db + "'"); }
            }
            rest = rest.substr(0, slash);
        }

        if (const auto at = rest.rfind('@'); at != std::string::npos) {
            const std::string credentials = rest.substr(0, at);
            const auto colon = credentials.find(':');
            if (colon == std::string::npos) {
                redis.password = percentDecode(credentials);          // redis://password@host
            } else {
                redis.username = percentDecode(credentials.substr(0, colon));
                redis.password = percentDecode(credentials.substr(colon + 1));
                if (redis.username == "default") redis.username.clear();   // AUTH <password> is the same thing
            }
            rest = rest.substr(at + 1);
        }
        if (const auto colon = rest.rfind(':'); colon != std::string::npos) {
            redis.port = std::stoi(rest.substr(colon + 1));
            rest = rest.substr(0, colon);
        }
        redis.host = rest;
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
    // Either one connection URL (Render) or separate fields (local .env).
    s->db.url         = dotenv::get_or("DB_URL", "");
    if (s->db.url.empty()) {
        s->db.host     = dotenv::require("DB_HOST");
        s->db.port     = toInt("DB_PORT", "5432");
        s->db.name     = dotenv::require("DB_NAME");
        s->db.user     = dotenv::require("DB_USER");
        s->db.password = dotenv::require("DB_PASSWORD");
    }
    s->db.schema      = dotenv::get_or("DB_SCHEMA", "public");
    s->db.connections = toInt("DB_CONNECTIONS", "4");
    s->jwt.accessSecret  = dotenv::require("JWT_ACCESS_SECRET");
    s->jwt.refreshSecret = dotenv::require("JWT_REFRESH_SECRET");
    s->jwt.accessExpiration  = dotenv::require("JWT_ACCESS_EXPIRATION");
    s->jwt.refreshExpiration = dotenv::require("JWT_REFRESH_EXPIRATION");
    s->redis.host        = dotenv::get_or("REDIS_HOST", "127.0.0.1");
    s->redis.port        = toInt("REDIS_PORT", "6379");
    s->redis.password    = dotenv::get_or("REDIS_PASSWORD", "");
    s->redis.username    = dotenv::get_or("REDIS_USERNAME", "");
    s->redis.db          = toInt("REDIS_DB", "0");
    if (auto url = dotenv::get("REDIS_URL"); url && !url->empty()) applyRedisUrl(*url, s->redis);   // Render
    s->redis.host        = resolveHost("REDIS_HOST", s->redis.host);
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
    s->uploads.pdfMaxBytes  = static_cast<size_t>(toInt("PDF_MAX_MB", "20")) * 1024 * 1024;
    s->uploads.pdfMaxPages  = toInt("PDF_MAX_PAGES", "2000");
    s->uploads.pdfTimeoutSeconds = toInt("PDF_TIMEOUT_SECONDS", "600");

    if (s->port < 1 || s->port > 65535) throw std::runtime_error("PORT out of range");

    g_settings = std::move(s);
    return g_settings.get();
}