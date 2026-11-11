#pragma once
#include <drogon/HttpMiddleware.h>
#include "services/Services.hpp"

class BookOwnerOrAdminMiddleware : public drogon::HttpCoroMiddleware<BookOwnerOrAdminMiddleware, false>
{
public:
    explicit BookOwnerOrAdminMiddleware(Services* service) : services_(service) {}

    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req,
                                                 drogon::MiddlewareNextAwaiter&& next) override;

private:
    Services* services_;
};
