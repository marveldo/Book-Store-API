#pragma once
#include "utils/Validation.hpp"
#include <regex>
#include <string>
#include <string_view>

struct CreatePaymentInput {
    std::string amount;      // "24.98": PaymentService must check it equals the order's total_price
};

inline ParseResult<CreatePaymentInput> parseCreatePayment(std::string_view raw) {
    static const std::regex money(R"(\d{1,8}(\.\d{1,2})?)");
    return parseInput<CreatePaymentInput>(raw, [](Validator& v) {
        v.onlyAllow({"amount"});
        auto amount = v.requiredString("amount", 1, 11);
        v.matches(amount, "amount", money, "must look like 24.98 (max 2 decimal places)");
        return CreatePaymentInput{amount.value_or("")};
    });
}
