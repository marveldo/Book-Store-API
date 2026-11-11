#pragma once
#include "utils/Validation.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct TocEntry {
    std::string title;
    int32_t     level;
    int32_t     pageNumber;
};

struct ReplaceTocInput {
    std::vector<TocEntry> entries;
};

inline ParseResult<ReplaceTocInput> parseReplaceToc(std::string_view raw) {
    auto body = nlohmann::json::parse(raw, nullptr, false);
    if (body.is_discarded()) return std::vector<FieldError>{{"body", "is not valid JSON"}};
    if (!body.is_object())   return std::vector<FieldError>{{"body", "must be a JSON object"}};

    Validator top(body);
    top.onlyAllow({"entries"});
    if (!top.ok()) return top.errors();
    if (!body.contains("entries") || !body["entries"].is_array()) return std::vector<FieldError>{{"entries", "must be an array"}};
    if (body["entries"].size() > 2000) return std::vector<FieldError>{{"entries", "must have at most 2000 items"}};

    ReplaceTocInput in;
    std::vector<FieldError> errors;
    for (size_t i = 0; i < body["entries"].size(); i++) {
        const std::string prefix = "entries[" + std::to_string(i) + "]";
        const auto& item = body["entries"][i];
        if (!item.is_object()) {
            errors.push_back({prefix, "must be an object"});
            continue;
        }

        Validator v(item);
        v.onlyAllow({"title", "level", "page_number"});
        auto title      = v.requiredString("title", 1, 500);
        auto level      = v.optionalInt("level", 0, 10);
        auto pageNumber = v.requiredInt("page_number", 1, 100000);

        for (size_t j = 0; j < v.errors().size(); j++) {
            errors.push_back({prefix + "." + v.errors()[j].field, v.errors()[j].message});
        }
        if (v.ok()) {
            in.entries.push_back({*title, static_cast<int32_t>(level.value_or(0)), static_cast<int32_t>(*pageNumber)});
        }
    }

    if (!errors.empty()) return errors;
    return in;
}
