#pragma once
#include "utils/Validation.hpp"
#include <regex>
#include <string>
#include <string_view>

struct PayOrderInput {
    std::string callbackUrl;   // the frontend page Paystack sends the customer back to
};

inline ParseResult<PayOrderInput> parsePayOrder(std::string_view raw) {
    static const std::regex url(R"(https?://[^\s"'<>]+)");
    return parseInput<PayOrderInput>(raw, [](Validator& v) {
        v.onlyAllow({"callback_url"});
        auto callbackUrl = v.requiredString("callback_url", 8, 500);
        v.matches(callbackUrl, "callback_url", url, "must be an http:// or https:// URL");
        return PayOrderInput{callbackUrl.value_or("")};
    });
}
