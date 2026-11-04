#pragma once
#include <drogon/HttpMiddleware.h>

class AccountOwnerMiddleware : public drogon::HttpMiddleware<AccountOwnerMiddleware>
{
public:
    void invoke(const drogon::HttpRequestPtr& req,
                drogon::MiddlewareNextCallback&& nextCb,
                drogon::MiddlewareCallback&& mcb) override;
};
