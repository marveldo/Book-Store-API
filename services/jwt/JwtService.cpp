#include "JwtService.hpp"
#include <jwt-cpp/traits/nlohmann-json/defaults.h>



namespace {
     auto conversion = [](const std::string& str) -> std::chrono::seconds {
        if (str.empty()) return std::chrono::seconds(0);
        char unit = str.back();
        int value = std::stoi(str.substr(0, str.size() - 1));
        switch (unit) {
            case 's': return std::chrono::seconds(value);
            case 'm': return std::chrono::minutes(value);
            case 'h': return std::chrono::hours(value);
            case 'd': return std::chrono::hours(value * 24);
            default: throw std::invalid_argument("Invalid time unit in expiration string");
      }};
}

std::string JwtService::generateAccessToken(Claims& claims) {
     std::string token =jwt::create()
            .set_type("JWS")
            .set_subject(claims.userId)
            .set_payload_claim("user_id", jwt::claim(claims.userId))
            .set_payload_claim("tokenType", jwt::claim(claims.tokenType))
            .set_payload_claim("email", jwt::claim(claims.email))
            .set_payload_claim("role", jwt::claim(toString(claims.role)))
            .set_expires_at(std::chrono::system_clock::now() + conversion(accessExpiration_))
            .sign(jwt::algorithm::hs256{accessSecret_});

    return token;

}

std::string JwtService::generateRefreshToken(Claims& claims) {
    std::string token = jwt::create()
            .set_type("JWS")
            .set_subject(claims.userId)
            .set_payload_claim("user_id", jwt::claim(claims.userId))
            .set_payload_claim("tokenType", jwt::claim(claims.tokenType))
            .set_payload_claim("email", jwt::claim(claims.email))
            .set_payload_claim("role", jwt::claim(toString(claims.role)))
            .set_expires_at(std::chrono::system_clock::now() + conversion(refreshExpiration_))
            .sign(jwt::algorithm::hs256{refreshSecret_});

    return token;
}

std::shared_ptr<Claims> JwtService::verifyAccessToken(const std::string& token){

    auto decoded = jwt::decode(token);
    std::shared_ptr<Claims> claims = std::make_shared<Claims>();
    
    auto verifier = jwt::verify().
                    allow_algorithm(jwt::algorithm::hs256{accessSecret_});

    verifier.verify(decoded);

    auto json_claims = decoded.get_payload_json();

    claims -> email = json_claims["email"].get<std::string>();
    claims -> userId = json_claims["user_id"].get<std::string>();
    claims -> tokenType = json_claims["tokenType"].get<std::string>();
    auto role = roleFromString(json_claims["role"].get<std::string>());
    if (!role) throw std::runtime_error("invalid role in token");
    claims -> role = *role;

    return claims;
}

std::shared_ptr<Claims> JwtService::verifyRefreshToken(const std::string& token){
    auto decoded = jwt::decode(token);
    std::shared_ptr<Claims> claims = std::make_shared<Claims>();
    
    auto verifier = jwt::verify().
                    allow_algorithm(jwt::algorithm::hs256{refreshSecret_});

    verifier.verify(decoded);

    auto json_claims = decoded.get_payload_json();

    claims -> email = json_claims["email"].get<std::string>();
    claims -> userId = json_claims["user_id"].get<std::string>();
    claims -> tokenType = json_claims["tokenType"].get<std::string>();
    auto role = roleFromString(json_claims["role"].get<std::string>());
    if (!role) throw std::runtime_error("invalid role in token");
    claims -> role = *role;

    return claims;
}




// std::shared_ptr<Claims> JwtService::verifyToken(const std::string& token) {
//     try {
//         auto decoded = jwt::decode(token);
//         std::shared_ptr<Claims> claims = std::make_shared<Claims>();
//         claims->userId = decoded.get_subject();
//         claims->tokenType = decoded.get_payload_claim("tokenType").as_string();
//         claims->email = decoded.get_payload_claim("email").as_string();
//         return claims;
//     } catch (const std::exception& e) {
//         return nullptr;
//     }
// }