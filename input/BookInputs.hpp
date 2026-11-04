#pragma once
#include "utils/Validation.hpp"
#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

struct CreateBookInput {
    std::string                title;
    std::string                author;
    std::string                price;            // "12.99": a string, because NUMERIC must not go through a float
    std::optional<std::string> isbn;
    std::optional<std::string> description;
    std::optional<int64_t>     publishedYear;
};

struct UpdateBookInput {                         // every field optional: nullopt = leave unchanged
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::optional<std::string> price;
    std::optional<std::string> isbn;
    std::optional<std::string> description;
    std::optional<int64_t>     publishedYear;
};

namespace book_rules {
    inline const std::regex price(R"(\d{1,8}(\.\d{1,2})?)");    
    inline const std::regex isbn(R"(\d{9}[\dX]|\d{13})");      
}

inline ParseResult<CreateBookInput> parseCreateBook(std::string_view raw) {
    return parseInput<CreateBookInput>(raw, [](Validator& v) {
        v.onlyAllow({"title", "author", "price", "isbn", "description", "published_year"});
        auto title       = v.requiredString("title", 1, 255);
        auto author      = v.requiredString("author", 1, 255);
        auto price       = v.requiredString("price", 1, 11);
        auto isbn        = v.optionalString("isbn", 10, 13);
        auto description = v.optionalString("description", 1, 5000);
        auto year        = v.optionalInt("published_year", 1, 9999);
        v.matches(price, "price", book_rules::price, "must look like 12.99 (max 2 decimal places)");
        v.matches(isbn,  "isbn",  book_rules::isbn,  "must be 10 or 13 digits, no dashes");
        return CreateBookInput{title.value_or(""), author.value_or(""), price.value_or(""),
                               isbn, description, year};
    });
}

inline ParseResult<UpdateBookInput> parseUpdateBook(std::string_view raw) {
    return parseInput<UpdateBookInput>(raw, [](Validator& v) {
        v.onlyAllow({"title", "author", "price", "isbn", "description", "published_year"});
        UpdateBookInput in{
            v.optionalString("title", 1, 255),
            v.optionalString("author", 1, 255),
            v.optionalString("price", 1, 11),
            v.optionalString("isbn", 10, 13),
            v.optionalString("description", 1, 5000),
            v.optionalInt("published_year", 1, 9999),
        };
        v.matches(in.price, "price", book_rules::price, "must look like 12.99 (max 2 decimal places)");
        v.matches(in.isbn,  "isbn",  book_rules::isbn,  "must be 10 or 13 digits, no dashes");
        if (v.ok() && !in.title && !in.author && !in.price && !in.isbn && !in.description && !in.publishedYear)
            v.addError("body", "must contain at least one field to update");
        return in;
    });
}
