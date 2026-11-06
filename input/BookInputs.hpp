#pragma once
#include "utils/Validation.hpp"
#include "utils/Money.hpp"
#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

struct CreateBookInput {
    std::string                title;
    std::string                author;
    int64_t                    price;
    std::optional<std::string> isbn;
    std::optional<std::string> description;
    std::optional<int64_t>     publishedYear;
};

struct UpdateBookInput {                         // every field optional: nullopt = leave unchanged
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::optional<int64_t>     price;
    std::optional<std::string> isbn;
    std::optional<std::string> description;
    std::optional<int64_t>     publishedYear;
};

namespace book_rules {
    inline const std::regex isbn(R"(\d{9}[\dX]|\d{13})");      
}

inline ParseResult<CreateBookInput> parseCreateBook(std::string_view raw) {
    return parseInput<CreateBookInput>(raw, [](Validator& v) {
        v.onlyAllow({"title", "author", "price", "isbn", "description", "published_year"});
        auto title       = v.requiredString("title", 1, 255);
        auto author      = v.requiredString("author", 1, 255);
        auto price       = v.requiredNumber("price", 0, 10000000);
        auto isbn        = v.optionalString("isbn", 10, 13);
        auto description = v.optionalString("description", 1, 5000);
        auto year        = v.optionalInt("published_year", 1, 9999);
        v.matches(isbn,  "isbn",  book_rules::isbn,  "must be 10 or 13 digits, no dashes");
        return CreateBookInput{title.value_or(""), author.value_or(""), money::toMinor(price.value_or(0)),
                               isbn, description, year};
    });
}

inline ParseResult<UpdateBookInput> parseUpdateBook(std::string_view raw) {
    return parseInput<UpdateBookInput>(raw, [](Validator& v) {
        v.onlyAllow({"title", "author", "price", "isbn", "description", "published_year"});
        UpdateBookInput in{
            v.optionalString("title", 1, 255),
            v.optionalString("author", 1, 255),
            std::nullopt,
            v.optionalString("isbn", 10, 13),
            v.optionalString("description", 1, 5000),
            v.optionalInt("published_year", 1, 9999),
        };
        if (auto price = v.optionalNumber("price", 0, 10000000)) in.price = money::toMinor(*price);
        v.matches(in.isbn,  "isbn",  book_rules::isbn,  "must be 10 or 13 digits, no dashes");
        if (v.ok() && !in.title && !in.author && !in.price && !in.isbn && !in.description && !in.publishedYear)
            v.addError("body", "must contain at least one field to update");
        return in;
    });
}
