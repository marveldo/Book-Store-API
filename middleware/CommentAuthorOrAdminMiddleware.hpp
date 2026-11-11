#pragma once
#include <drogon/HttpMiddleware.h>
#include "services/Services.hpp"

class CommentAuthorOrAdminMiddleware : public drogon::HttpCoroMiddleware<CommentAuthorOrAdminMiddleware, false>
{
public:
    explicit CommentAuthorOrAdminMiddleware(Services* service) : services_(service) {}

    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req,
                                                 drogon::MiddlewareNextAwaiter&& next) override;

private:
    Services* services_;
};
