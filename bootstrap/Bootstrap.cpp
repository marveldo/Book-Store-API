#include "Bootstrap.hpp"
#include "utils/Security.hpp"
#include "queues/handlers/TaskHandlers.hpp"
#include "events/RegisterEvents.hpp"
#include <string>

AppContext* bootstrap() {
    AppContext* ctx = new AppContext();
    try {
        ctx->settings = new Settings(*getSettings());
        security::init();

        const DatabaseSettings& db = ctx->settings->db;
        ctx->db = drogon::orm::DbClient::newPgClient(
            "host=" + db.host +
            " port=" + std::to_string(db.port) +
            " dbname=" + db.name +
            " user=" + db.user +
            " password=" + db.password,
            db.connections);

        ctx->events = new EventBus();

        const RedisSettings& redis = ctx->settings->redis;
        ctx->queue = new TaskQueue(redis.host, redis.port, redis.password, redis.maxAttempts, redis.maxInFlight);

        const EmailSettings& email = ctx->settings->email;
        ctx->email = new EmailService(email.resendApiKey, email.from, email.templatesDir, email.appUrl, email.assetsUrl, ctx->queue->loop(), redis.maxInFlight);

        registerTaskHandlers(ctx->queue, ctx->email, getServices(ctx->db, *ctx->settings, ctx->events));
        RegisterEvents(ctx->events, ctx->queue);
        ctx->queue->start();
    } catch (...) {
        delete ctx;
        throw;
    }
    return ctx;
}
