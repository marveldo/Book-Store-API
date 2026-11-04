#include "Bootstrap.hpp"
#include "utils/Security.hpp"
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
    } catch (...) {
        delete ctx;
        throw;
    }
    return ctx;
}
