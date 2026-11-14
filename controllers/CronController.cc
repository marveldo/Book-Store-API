#include "CronController.h"
#include "Api.hpp"
#include <openssl/crypto.h>

bool CronController::validSecret(const std::string& given) const {
    return !secret_.empty() && given.size() == secret_.size() &&
           CRYPTO_memcmp(given.data(), secret_.data(), secret_.size()) == 0;
}

Task<HttpResponsePtr> CronController::Run(HttpRequestPtr req, std::string task) {
    if (!validSecret(req->getHeader("x-cron-secret"))) co_return res::error(k401Unauthorized, "Bad cron secret");
    if (!scheduler_->knows(task)) co_return res::notFound("Unknown cron task");

    Json::Value data;
    data["task"] = task;
    data["enqueued"] = co_await scheduler_->trigger(task);
    co_return res::success(data, k202Accepted);
}
