#include "UsersController.h"
#include "input/UserInputs.hpp"

// Add definition of your processing function here


Task<HttpResponsePtr> UsersController::CreateUser(HttpRequestPtr req) {
    Json::Value user;
    auto parseResult = parseCreateUser(req->body());
    if (std::holds_alternative<std::vector<FieldError>>(parseResult)) {
        auto errors = std::get<std::vector<FieldError>>(parseResult);
        co_return res::badRequest(errors);
    }

    auto input = std::get<CreateUserInput>(parseResult);
    
    auto createdUser = co_await services_->userService->createUser(input);
     
    user = createdUser.toJson();
    user.removeMember("password_hash");

    co_return res::success(user, drogon::k201Created);
    
    
}

Task<HttpResponsePtr> UsersController::GetAllUsers(HttpRequestPtr req) {
    Json::Value users(Json::arrayValue);
    auto allUsers = co_await services_->userService->getAllUsers();
    for (const auto& user : allUsers) {
        auto userJson = user.toJson();
        userJson.removeMember("password_hash");
        users.append(userJson);
    }
    co_return res::success(users);
}

Task<HttpResponsePtr> UsersController::GetUserById(HttpRequestPtr req, std::string id) {
    Json::Value user;
    auto foundUser = co_await services_->userService->getUserById(id);
    user = foundUser.toJson();
    user.removeMember("password_hash");
    co_return res::success(user);
}

Task<HttpResponsePtr> UsersController::UpdateUser(HttpRequestPtr req, std::string id) {
    Json::Value user;
    auto parseResult = parseUpdateUser(req->body());
    if (std::holds_alternative<std::vector<FieldError>>(parseResult)) {
        auto errors = std::get<std::vector<FieldError>>(parseResult);
        co_return res::badRequest(errors);
    }

    auto input = std::get<UpdateUserInput>(parseResult);

    
    auto updatedUser = co_await services_->userService->updateUser(id, input);
     
    user = updatedUser.toJson();
    user.removeMember("password_hash");

    co_return res::success(user);
}

Task<HttpResponsePtr> UsersController::DeleteUser(HttpRequestPtr req, std::string id) {
    bool deleted = co_await services_->userService->deleteUser(id);
    if (deleted) {
        co_return res::success(Json::Value(), drogon::k200OK);
    } else {
        co_return res::notFound("User not found");
    }
}