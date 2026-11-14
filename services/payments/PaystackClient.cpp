#include "PaystackClient.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <cstdio>
#include <stdexcept>

namespace {
    std::string toHex(const unsigned char* bytes, unsigned int length) {
        std::string hex;
        hex.reserve(length * 2);
        char byte[3];
        for (unsigned int i = 0; i < length; i++) {
            std::snprintf(byte, sizeof(byte), "%02x", bytes[i]);
            hex += byte;
        }
        return hex;
    }
}

PaystackClient::PaystackClient(std::string secretKey, std::string apiUrl)
    : secret_(std::move(secretKey)),
      http_(drogon::HttpClient::newHttpClient(apiUrl)) {}

drogon::HttpRequestPtr PaystackClient::request(drogon::HttpMethod method, const std::string& path) const {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(method);
    req->setPath(path);
    req->addHeader("Authorization", "Bearer " + secret_);
    return req;
}

nlohmann::json PaystackClient::dataOf(const drogon::HttpResponsePtr& resp) {
    auto body = nlohmann::json::parse(resp->body(), nullptr, false);
    if (resp->statusCode() != drogon::k200OK || !body.is_object() || !body.value("status", false)) {
        const std::string message = body.is_object() ? body.value("message", "") : "";
        throw std::runtime_error("Paystack " + std::to_string(static_cast<int>(resp->statusCode())) + ": " + message);
    }
    return body.at("data");
}

drogon::Task<ProviderCheckout> PaystackClient::initialize(const std::string& email, int64_t amount, const std::string& reference,
                                                          const std::string& callbackUrl) {
    auto req = request(drogon::Post, "/transaction/initialize");
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(nlohmann::json{
        {"email", email},
        {"amount", amount},
        {"currency", "NGN"},
        {"reference", reference},
        {"callback_url", callbackUrl},
    }.dump());

    const auto data = dataOf(co_await http_->sendRequestCoro(req, 15));
    co_return ProviderCheckout{data.at("authorization_url").get<std::string>()};
}

drogon::Task<ProviderStatus> PaystackClient::verify(const std::string& reference) {
    const auto data = dataOf(co_await http_->sendRequestCoro(request(drogon::Get, "/transaction/verify/" + reference), 15));
    co_return ProviderStatus{
        data.at("status").get<std::string>(),
        data.at("amount").get<int64_t>(),
        data.at("currency").get<std::string>(),
    };
}

drogon::Task<> PaystackClient::refund(const std::string& reference) {
    auto req = request(drogon::Post, "/refund");
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(nlohmann::json{{"transaction", reference}}.dump());

    const auto resp = co_await http_->sendRequestCoro(req, 15);
    const auto body = nlohmann::json::parse(resp->body(), nullptr, false);
    const bool ok = body.is_object() && body.value("status", false);
    const std::string message = body.is_object() ? body.value("message", "") : "";

    if (ok || message.find("fully reversed") != std::string::npos) co_return;
    throw std::runtime_error("Paystack refund " + std::to_string(static_cast<int>(resp->statusCode())) + ": " + message);
}

bool PaystackClient::validSignature(std::string_view body, std::string_view signature) const {
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    HMAC(EVP_sha512(), secret_.data(), static_cast<int>(secret_.size()),
         reinterpret_cast<const unsigned char*>(body.data()), body.size(), mac, &length);
    const std::string expected = toHex(mac, length);
    return signature.size() == expected.size() &&
           CRYPTO_memcmp(expected.data(), signature.data(), expected.size()) == 0;
}
