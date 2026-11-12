#include "AuthController.h"
#include "input/UserInputs.hpp"
#include <drogon/orm/Exception.h>
#include <optional>

Task<HttpResponsePtr> AuthController::Login(HttpRequestPtr req) {
    auto parseResult = parseLoginCredentials(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parseResult)) {
        co_return res::badRequest(*errors);
    }
    const auto& input = std::get<LoginUserInput>(parseResult);

    std::optional<drogon_model::bookstore::Users> user;
    try {
        user = co_await services_->userService->getUserByCredentials(input);
    } catch (const drogon::orm::UnexpectedRows&) {
    }

    if (!user || !co_await services_->userService->verifyPassword(input.password, user->getValueOfPasswordHash())) {
        co_return res::error(drogon::k401Unauthorized, "Invalid email or password");
    }

    co_return res::success(tokensFor(*user));
}

Task<HttpResponsePtr> AuthController::Refresh(HttpRequestPtr req) {
    auto parseResult = parseRefreshToken(req->body());
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parseResult)) {
        co_return res::badRequest(*errors);
    }
    const auto& input = std::get<RefreshTokenInput>(parseResult);

    // Checks the signature (refresh secret) and the expiry. An access token fails here too,
    // because it's signed with the other secret.
    std::shared_ptr<Claims> claims;
    try {
        claims = services_->jwtService->verifyRefreshToken(input.refreshToken);
    } catch (const std::exception&) {
    }
    if (!claims || claims->tokenType != "refresh") {
        co_return res::error(drogon::k401Unauthorized, "Invalid or expired refresh token");
    }

    // From the database, not the token: a deleted account can't refresh, and a changed
    // email or role shows up in the new tokens.
    std::optional<drogon_model::bookstore::Users> user;
    try {
        user = co_await services_->userService->getUserById(claims->userId);
    } catch (const drogon::orm::UnexpectedRows&) {
    }
    if (!user) {
        co_return res::error(drogon::k401Unauthorized, "This account no longer exists");
    }

    co_return res::success(tokensFor(*user));
}

Json::Value AuthController::tokensFor(const drogon_model::bookstore::Users& user) {
    Role role = roleFromString(user.getValueOfRole()).value_or(Role::User);
    Claims accessClaims{user.getValueOfId(), "access", user.getValueOfEmail(), role};
    Claims refreshClaims{user.getValueOfId(), "refresh", user.getValueOfEmail(), role};

    Json::Value userJson = user.toJson();
    userJson.removeMember("password_hash");

    Json::Value data;
    data["access_token"]  = services_->jwtService->generateAccessToken(accessClaims);
    data["refresh_token"] = services_->jwtService->generateRefreshToken(refreshClaims);
    data["token_type"]    = "Bearer";
    data["user"]          = userJson;
    return data;
}
