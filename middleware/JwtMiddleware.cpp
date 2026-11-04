#include "JwtMiddleware.hpp"
#include "controllers/Api.hpp"

using namespace drogon;


void JwtMiddleWare::invoke(const drogon::HttpRequestPtr &req, 
                            drogon::MiddlewareNextCallback &&nextCb, 
                            drogon::MiddlewareCallback &&mcb) {
    
      auto &authorization = req->getHeader("Authorization");

      if(authorization.empty() || authorization.rfind(TOKEN_PREFIX , 0) != 0){
        auto resp = res::error(k401Unauthorized , "Authorization Key Empty");
        mcb(resp);
        return;
      }
     

    try {
        auto extractBearer = [&]() -> std::string {
             if (authorization.size() > TOKEN_PREFIX.size() && authorization.substr(0, TOKEN_PREFIX.size()) == TOKEN_PREFIX) {
             std::string raw_token = std::string(authorization.substr(TOKEN_PREFIX.size()));
            
              // Trim trailing spaces, newlines, or carriage returns (\r\n)
             size_t end = raw_token.find_last_not_of(" \t\r\n");
             if (end != std::string::npos) {
                raw_token.erase(end + 1);
             }
             return raw_token;
         }
           return "";
       };
        std::string token_string = extractBearer();
        if (token_string.empty()) {
            auto resp = res::error(k401Unauthorized , "Couldnt Extract Key");
            mcb(resp);
            return ;
        }
        auto claims = services_ ->jwtService->verifyAccessToken(token_string);
        
        req-> attributes() -> insert("id", claims->userId);

        req -> attributes() -> insert("email", claims -> email );

        req -> attributes() -> insert("role", claims -> role );
      }catch(std::exception& error){
            auto resp = res::error(k401Unauthorized , "Invalid Authorization Keys");
            LOG_ERROR << std::format("Error encountered {}", error.what());
            mcb(resp);
            return ;
      }


      nextCb([mcb = std::move(mcb)](const HttpResponsePtr &resp){
          mcb(resp);
      });
    }