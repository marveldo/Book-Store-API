#include "Bootstrap.hpp"
#include "utils/Security.hpp"
#include "queues/handlers/TaskHandlers.hpp"
#include "events/RegisterEvents.hpp"
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <chrono>
#include <filesystem>
#include <string>

namespace {
    // PDFs wait in uploads/pdfs until their extract_pdf task deletes them. If the server
    // stopped first (or Redis lost the task), nothing would. An hour is far longer than
    // any extraction takes, so older files are leftovers.
    void removeStalePdfs() {
        namespace fs = std::filesystem;
        const fs::path dir = fs::path(drogon::app().getUploadPath()) / "pdfs";
        const auto cutoff = fs::file_time_type::clock::now() - std::chrono::hours(1);

        std::error_code ec;
        size_t removed = 0;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec) || entry.path().extension() != ".pdf") continue;
            if (entry.last_write_time(ec) < cutoff && fs::remove(entry.path(), ec)) removed++;
        }
        if (removed) LOG_INFO << "Removed " << removed << " leftover PDF upload(s)";
    }
}

AppContext* bootstrap() {
    AppContext* ctx = new AppContext();
    try {
        ctx->settings = new Settings(*getSettings());
        security::init();

        const DatabaseSettings& db = ctx->settings->db;
        // libpq accepts a postgresql:// URL as well as "key=value" pairs. Values are quoted so a
        // password with spaces, quotes or backslashes doesn't break the string.
        const auto quoted = [](const std::string& value) {
            std::string out = "'";
            for (char c : value) {
                if (c == '\\' || c == '\'') out += '\\';
                out += c;
            }
            return out + "'";
        };
        const std::string connection = !db.url.empty() ? db.url :
            "host=" + quoted(db.host) +
            " port=" + std::to_string(db.port) +
            " dbname=" + quoted(db.name) +
            " user=" + quoted(db.user) +
            " password=" + quoted(db.password);
        ctx->db = drogon::orm::DbClient::newPgClient(connection, db.connections);

        ctx->events = new EventBus();

        const RedisSettings& redis = ctx->settings->redis;
        ctx->queue = new TaskQueue(redis.host, redis.port, redis.password, redis.maxAttempts, redis.maxInFlight,
                                   redis.username, redis.db);

        const EmailSettings& email = ctx->settings->email;
        ctx->email = new EmailService(email.resendApiKey, email.from, email.templatesDir, email.appUrl, email.assetsUrl, ctx->queue->loop(), redis.maxInFlight);

        registerTaskHandlers(ctx->queue, ctx->email, getServices(ctx->db, *ctx->settings, ctx->events));
        RegisterEvents(ctx->events, ctx->queue);
        removeStalePdfs();
        ctx->queue->start();
    } catch (...) {
        delete ctx;
        throw;
    }
    return ctx;
}
