#pragma once
#include "utils/Validation.hpp"
#include <string>
#include <string_view>

struct CommentInput {
    std::string content;
};

inline ParseResult<CommentInput> parseComment(std::string_view raw) {
    return parseInput<CommentInput>(raw, [](Validator& v) {
        v.onlyAllow({"content"});
        auto content = v.requiredString("content", 1, 2000);
        return CommentInput{content.value_or("")};
    });
}
