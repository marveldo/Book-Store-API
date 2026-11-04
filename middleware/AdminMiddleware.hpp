#pragma once
#include <drogon/HttpMiddleware.h>
#include "services/Services.hpp"

class AdminMiddleware : public drogon::HttpCoroMiddleware<AdminMiddleware, false>
{
public:
    explicit AdminMiddleware(Services* service) : services_(service) {}

    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req,
                                                 drogon::MiddlewareNextAwaiter&& next) override;

private:
    Services* services_;
};
