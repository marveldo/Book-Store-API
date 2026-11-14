#pragma once

#include <drogon/HttpController.h>

#include "services/cron/Scheduler.hpp"

using namespace drogon;

class CronController : public drogon::HttpController<CronController, false>
{
  public:
    CronController(Scheduler* scheduler, std::string secret) : scheduler_(scheduler), secret_(std::move(secret)) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CronController::Run, "/internal/cron/{task}", Post);
    METHOD_LIST_END

    Task<HttpResponsePtr> Run(HttpRequestPtr req, std::string task);

  private:
    bool validSecret(const std::string& given) const;

    Scheduler* scheduler_;
    std::string secret_;
};
