#pragma once
#include "utils/Validation.hpp"
#include <cstddef>
#include <string>
#include <vector>

struct PageInput {
    size_t page    = 1;
    size_t perPage = 20;
};

inline ParseResult<PageInput> parsePageQuery(const std::string& page, const std::string& perPage) {
    PageInput in;
    std::vector<FieldError> errors;

    auto readNumber = [&](const std::string& raw, const std::string& field, size_t max, size_t& out) {
        if (raw.empty()) return;
        try {
            size_t used = 0;
            long long value = std::stoll(raw, &used);
            if (used != raw.size() || value < 1 || static_cast<size_t>(value) > max) throw std::out_of_range(field);
            out = static_cast<size_t>(value);
        } catch (const std::exception&) {
            errors.push_back({field, "must be a whole number between 1 and " + std::to_string(max)});
        }
    };

    readNumber(page, "page", 100000, in.page);
    readNumber(perPage, "per_page", 100, in.perPage);

    if (!errors.empty()) return errors;
    return in;
}
