#pragma once
#include "utils/Validation.hpp"
#include <cstdint>
#include <string>
#include <string_view>

// total_price is NOT accepted from the client: OrderService works it out from the book's price.
struct CreateOrderInput {
    std::string bookId;
    int64_t     quantity;
};

inline ParseResult<CreateOrderInput> parseCreateOrder(std::string_view raw) {
    return parseInput<CreateOrderInput>(raw, [](Validator& v) {
        v.onlyAllow({"book_id", "quantity"});
        auto bookId   = v.requiredString("book_id", 36, 36);   // UUID
        auto quantity = v.requiredInt("quantity", 1, 100);
        return CreateOrderInput{bookId.value_or(""), quantity.value_or(0)};
    });
}
