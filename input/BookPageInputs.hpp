#pragma once
#include "utils/Validation.hpp"
#include <cstdint>
#include <string>
#include <string_view>

struct CreatePageInput {
    int32_t     pageNumber;
    std::string content;
};

struct UpdatePageInput {
    std::string content;
};

inline ParseResult<CreatePageInput> parseCreatePage(std::string_view raw) {
    return parseInput<CreatePageInput>(raw, [](Validator& v) {
        v.onlyAllow({"page_number", "content"});
        auto pageNumber = v.requiredInt("page_number", 1, 100000);
        auto content    = v.requiredString("content", 1, 100000);
        return CreatePageInput{static_cast<int32_t>(pageNumber.value_or(1)), content.value_or("")};
    });
}

inline ParseResult<UpdatePageInput> parseUpdatePage(std::string_view raw) {
    return parseInput<UpdatePageInput>(raw, [](Validator& v) {
        v.onlyAllow({"content"});
        auto content = v.requiredString("content", 1, 100000);
        return UpdatePageInput{content.value_or("")};
    });
}
