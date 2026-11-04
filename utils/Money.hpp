#pragma once
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace money {

inline int64_t toCents(const std::string& amount) {
    const auto dot = amount.find('.');
    const std::string whole = amount.substr(0, dot);
    std::string fraction = dot == std::string::npos ? "" : amount.substr(dot + 1);
    if (whole.empty() || fraction.size() > 2) throw std::invalid_argument("invalid amount: " + amount);
    fraction.resize(2, '0');
    return std::stoll(whole) * 100 + std::stoll(fraction);
}

inline std::string fromCents(int64_t cents) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld.%02lld",
                  static_cast<long long>(cents / 100), static_cast<long long>(cents % 100));
    return buf;
}

}
