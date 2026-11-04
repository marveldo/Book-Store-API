#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

struct FieldError {
    std::string field;
    std::string message;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FieldError, field, message)

template <typename T>
using ParseResult = std::variant<T, std::vector<FieldError>>;

class Validator {
public:
    explicit Validator(const nlohmann::json& body) : body_(body) {}

    std::optional<std::string> requiredString(const std::string& field, size_t minLen, size_t maxLen) {
        if (!present(field))            return std::nullopt;
        if (!body_[field].is_string())  return fail(field, "must be a string");
        auto value = body_[field].get<std::string>();
        if (value.size() < minLen)      return fail(field, "must be at least " + std::to_string(minLen) + " characters");
        if (value.size() > maxLen)      return fail(field, "must be at most "  + std::to_string(maxLen) + " characters");
        return value;
    }

    void matches(const std::optional<std::string>& value, const std::string& field,
                 const std::regex& pattern, const std::string& message) {
        if (value && !std::regex_match(*value, pattern)) fail(field, message);
    }

    std::optional<int64_t> requiredInt(const std::string& field,
                                       int64_t min = std::numeric_limits<int64_t>::min(),
                                       int64_t max = std::numeric_limits<int64_t>::max()) {
        if (!present(field))                    return std::nullopt;
        const auto& v = body_[field];
        if (!v.is_number_integer())             return fail(field, "must be a whole number");
        if (v.is_number_unsigned() && v.get<uint64_t>() > uint64_t(std::numeric_limits<int64_t>::max()))
                                                return fail(field, "is too large");
        auto value = v.get<int64_t>();
        if (value < min)                        return fail(field, "must be at least " + std::to_string(min));
        if (value > max)                        return fail(field, "must be at most "  + std::to_string(max));
        return value;
    }

    template <typename E>
    std::optional<E> requiredEnum(const std::string& field,
                                  const std::vector<std::pair<std::string, E>>& options) {
        if (!present(field))            return std::nullopt;
        if (!body_[field].is_string())  return fail(field, "must be a string");
        const auto value = body_[field].get<std::string>();
        for (const auto& [name, e] : options)
            if (name == value) return e;
        std::string allowed;
        for (const auto& [name, _] : options) allowed += (allowed.empty() ? "" : ", ") + name;
        return fail(field, "must be one of: " + allowed);
    }

    // ---- optional fields: absent or null = "not provided", no error ---------
    // (if present, the same rules as the required version apply)
    std::optional<std::string> optionalString(const std::string& field, size_t minLen, size_t maxLen) {
        if (!given(field)) return std::nullopt;
        return requiredString(field, minLen, maxLen);
    }

    std::optional<int64_t> optionalInt(const std::string& field,
                                       int64_t min = std::numeric_limits<int64_t>::min(),
                                       int64_t max = std::numeric_limits<int64_t>::max()) {
        if (!given(field)) return std::nullopt;
        return requiredInt(field, min, max);
    }

    template <typename E>
    std::optional<E> optionalEnum(const std::string& field,
                                  const std::vector<std::pair<std::string, E>>& options) {
        if (!given(field)) return std::nullopt;
        return requiredEnum<E>(field, options);
    }

    // ---- booleans: true / false only (not 1, 0 or "true") --------------------
    std::optional<bool> requiredBool(const std::string& field) {
        if (!present(field))            return std::nullopt;
        if (!body_[field].is_boolean()) return fail(field, "must be true or false");
        return body_[field].get<bool>();
    }

    std::optional<bool> optionalBool(const std::string& field) {
        if (!given(field)) return std::nullopt;
        return requiredBool(field);
    }

    // ---- custom rules: record your own error ----------------------------------
    void addError(const std::string& field, const std::string& message) { fail(field, message); }

    void onlyAllow(const std::set<std::string>& allowed) {
        for (const auto& [key, _] : body_.items())
            if (!allowed.count(key)) fail(key, "is not allowed");
    }

    bool ok() const { return errors_.empty(); }
    const std::vector<FieldError>& errors() const { return errors_; }

private:
    bool given(const std::string& field) const {
        return body_.contains(field) && !body_[field].is_null();
    }

    bool present(const std::string& field) {
        if (body_.contains(field) && !body_[field].is_null()) return true;   // null counts as missing
        fail(field, "is required");
        return false;
    }

    std::nullopt_t fail(const std::string& field, const std::string& message) {
        errors_.push_back({field, message});
        return std::nullopt;
    }

    const nlohmann::json& body_;
    std::vector<FieldError> errors_;
};

// Parse a raw request body and run your rules on it.
//   rules: a function (Validator&) -> T that reads the fields and builds T.
// Returns T only if the JSON is valid AND every rule passed; otherwise all the errors.
template <typename T, typename Rules>
ParseResult<T> parseInput(std::string_view rawBody, Rules&& rules) {
    auto body = nlohmann::json::parse(rawBody, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) return std::vector<FieldError>{{"body", "is not valid JSON"}};
    if (!body.is_object())   return std::vector<FieldError>{{"body", "must be a JSON object"}};

    Validator v(body);
    T input = rules(v);
    if (!v.ok()) return v.errors();
    return input;
}