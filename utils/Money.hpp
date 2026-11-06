#pragma once
#include <cmath>
#include <cstdint>

namespace money {

inline int64_t toMinor(double amount) {
    return std::llround(amount * 100.0);
}

inline double toMajor(int64_t minor) {
    return static_cast<double>(minor) / 100.0;
}

}
