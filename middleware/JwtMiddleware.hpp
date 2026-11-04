#pragma once 
#include <drogon/HttpMiddleware.h>
#include "services/Services.hpp"

namespace {
   const std::string TOKEN_PREFIX = "Bearer ";
}

class JwtMiddleWare : public drogon::HttpMiddleware<JwtMiddleWare, false>
{
public:
    JwtMiddleWare(Services* service) : services_(service){};

    void invoke(const drogon::HttpRequestPtr &req, 
                drogon::MiddlewareNextCallback &&nextCb, 
                drogon::MiddlewareCallback &&mcb) override;
    
private :
    
    Services* services_;
   
};