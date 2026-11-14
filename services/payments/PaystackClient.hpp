#pragma once
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <string_view>

struct ProviderCheckout {
    std::string authorizationUrl;
};

struct ProviderStatus {
    std::string status;
    int64_t amount = 0;
    std::string currency;
};

class PaystackClient {
public:
    PaystackClient(std::string secretKey, std::string apiUrl = "https://api.paystack.co");

    drogon::Task<ProviderCheckout> initialize(const std::string& email, int64_t amount, const std::string& reference,
                                              const std::string& callbackUrl);
    drogon::Task<ProviderStatus> verify(const std::string& reference);
    drogon::Task<> refund(const std::string& reference);
    bool validSignature(std::string_view body, std::string_view signature) const;

private:
    drogon::HttpRequestPtr request(drogon::HttpMethod method, const std::string& path) const;
    static nlohmann::json dataOf(const drogon::HttpResponsePtr& resp);

    std::string secret_;
    drogon::HttpClientPtr http_;
};
