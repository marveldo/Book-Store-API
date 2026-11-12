#include <drogon/drogon.h>
#include "settings/Settings.h"
#include "controllers/UsersController.h"
#include "controllers/BooksController.h"
#include "controllers/BookPagesController.h"
#include "controllers/BookTocController.h"
#include "controllers/BookCommentsController.h"
#include "controllers/OrdersController.h"
#include "controllers/PaymentsController.h"
#include "controllers/SettingsController.h"
#include "controllers/AuthController.h"
#include "controllers/JobsController.h"
#include "services/Services.hpp"
#include "bootstrap/Bootstrap.hpp"
#include "utils/Errors.hpp"
#include "utils/ApiDocs.hpp"
#include "utils/RateLimit.hpp"
#include "middleware/JwtMiddleware.hpp"
#include "middleware/AdminMiddleware.hpp"
#include "middleware/BookOwnerOrAdminMiddleware.hpp"
#include "middleware/CommentAuthorOrAdminMiddleware.hpp"


using namespace drogon;
int main() {
    
    AppContext* ctx = nullptr;

    try {
        ctx = bootstrap();
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to bootstrap application: " << e.what();
        return 1;
    }
    drogon::app().addListener(ctx->settings->host, ctx->settings->port);
    //Load config file
    //drogon::app().loadConfigFile("../config.json");
    //drogon::app().loadConfigFile("../config.yaml");
    //Run HTTP framework,the method will block in the internal event loop
    LOG_INFO << "Starting server on port " << ctx->settings->port << " with " << ctx->settings->threads << " threads in " << ctx->settings->environment << " mode.";
    drogon::app().setThreadNum(ctx->settings->threads);
    drogon::app().setFloatPrecisionInJson(2, "decimal");
    drogon::app().setClientMaxBodySize(ctx->settings->uploads.pdfMaxBytes + 1024 * 1024);
    drogon::app().registerPreSendingAdvice([](const HttpRequestPtr &req, const HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

        const auto us = trantor::Date::now().microSecondsSinceEpoch() - req->creationDate().microSecondsSinceEpoch();
        LOG_INFO << "Request: " << req->methodString() << " " << req->path() << ", Response: " << resp->statusCode()
                 << ", took " << us << "us";
    });
    drogon::app().registerHandler("/", [](HttpRequestPtr req) -> Task<HttpResponsePtr> {
        Json::Value json;
        json["message"] = "Welcome to the C++ Server!";
        json["status"]  = "success";
        co_return HttpResponse::newHttpJsonResponse(json); 
         
    },{Get});

    registerApiDocs();   // GET /openapi.json and GET /docs
    registerUserRateLimit();   // 120 requests a minute per logged-in user (route limits: middleware/RateLimitMiddlewares)

    Services* service = getServices(ctx->db, *ctx->settings, ctx->events);
    drogon::app().registerMiddleware(std::make_shared<JwtMiddleWare>(service));
    drogon::app().registerMiddleware(std::make_shared<AdminMiddleware>(service));
    drogon::app().registerMiddleware(std::make_shared<BookOwnerOrAdminMiddleware>(service));
    drogon::app().registerMiddleware(std::make_shared<CommentAuthorOrAdminMiddleware>(service));
    drogon::app().registerController(std::make_shared<UsersController>(service));
    drogon::app().registerController(std::make_shared<AuthController>(service));
    drogon::app().registerController(std::make_shared<BooksController>(service));
    drogon::app().registerController(std::make_shared<BookPagesController>(service));
    drogon::app().registerController(std::make_shared<BookTocController>(service));
    drogon::app().registerController(std::make_shared<BookCommentsController>(service));
    drogon::app().registerController(std::make_shared<OrdersController>(service));
    drogon::app().registerController(std::make_shared<PaymentsController>(service));
    drogon::app().registerController(std::make_shared<SettingsController>(service));
    drogon::app().registerController(std::make_shared<JobsController>(service));
    drogon::app().setExceptionHandler(handleException);
    drogon::app().setCustom404Page(res::notFound("Route not found"));
    drogon::app().run();

    delete ctx;
    return 0;
}
