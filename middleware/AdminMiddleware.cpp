#include "AdminMiddleware.hpp"
#include "controllers/Api.hpp"
#include "utils/Role.hpp"
#include <drogon/orm/Exception.h>
#include <optional>

using namespace drogon;

Task<HttpResponsePtr> AdminMiddleware::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    const std::string id = req->attributes()->get<std::string>("id");
    if (id.empty()) {
        co_return res::error(k401Unauthorized, "Not authenticated");
    }

    std::optional<drogon_model::bookstore::Users> user;
    try {
        user = co_await services_->userService->getUserById(id);
    } catch (const drogon::orm::UnexpectedRows&) {
    }

    if (!user || !user->getValueOfIsActive()) {
        co_return res::error(k401Unauthorized, "Account not found");
    }
    if (roleFromString(user->getValueOfRole()) != Role::Admin) {
        co_return res::error(k403Forbidden, "Admins only");
    }

    co_return co_await next;
}
