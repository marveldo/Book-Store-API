#pragma once
#include <drogon/orm/Exception.h>
#include <string_view>

inline bool isUniqueViolation(const drogon::orm::Failure& e) {
    return std::string_view(e.what()).find("duplicate key value violates unique constraint") != std::string_view::npos;
}

inline bool isForeignKeyViolation(const drogon::orm::Failure& e) {
    return std::string_view(e.what()).find("violates foreign key constraint") != std::string_view::npos;
}
