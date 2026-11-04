#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class UsersController : public drogon::HttpController<UsersController , false>
{
  public:

    explicit UsersController(Services* srv) : services_(srv) {}
    METHOD_LIST_BEGIN
    // use METHOD_ADD to add your custom processing function here;
    // METHOD_ADD(UsersController::get, "/{2}/{1}", Get); // path is /UsersController/{arg2}/{arg1}
    // METHOD_ADD(UsersController::your_method_name, "/{1}/{2}/list", Get); // path is /UsersController/{arg1}/{arg2}/list
    // ADD_METHOD_TO(UsersController::your_method_name, "/absolute/path/{1}/{2}/list", Get); // path is /absolute/path/{arg1}/{arg2}/list
    ADD_METHOD_TO(UsersController::CreateUser, API_V1 + "/users", Post);
    ADD_METHOD_TO(UsersController::GetAllUsers, API_V1 + "/users", Get);
    ADD_METHOD_VIA_REGEX(UsersController::GetUserById, API_V1 + "/users/([0-9a-fA-F-]{36})", Get , "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(UsersController::UpdateUser,  API_V1 + "/users/([0-9a-fA-F-]{36})", Put , "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(UsersController::DeleteUser,  API_V1 + "/users/([0-9a-fA-F-]{36})", Delete , "JwtMiddleWare");

    METHOD_LIST_END
    // your declaration of processing function maybe like this:
    // void get(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, int p1, std::string p2);
    // void your_method_name(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, double p1, int p2) const;

    Task<HttpResponsePtr> CreateUser(HttpRequestPtr req);
    Task<HttpResponsePtr> GetAllUsers(HttpRequestPtr req);
    Task<HttpResponsePtr> GetUserById(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> UpdateUser(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> DeleteUser(HttpRequestPtr req, std::string id);

    private:
      Services* services_;
};
