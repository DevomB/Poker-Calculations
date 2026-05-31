#pragma once

#include <cstdint>

namespace poker {

[[nodiscard]] inline int popcount_u32(std::uint32_t x) {
    int c = 0;
    while (x != 0) {
        ++c;
        x &= x - 1U;
    }
    return c;
}

[[nodiscard]] inline int popcount_u64(std::uint64_t x) {
    int c = 0;
    while (x != 0) {
        ++c;
        x &= x - 1ULL;
    }
    return c;
}

}  // namespace poker
