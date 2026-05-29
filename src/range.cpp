#include "poker/range.hpp"

#include "poker/deck_bitset.hpp"

#include <cmath>
#include <stdexcept>

namespace poker {

SparseRange sparse_range_from_arrays(const std::vector<int>& pair_indices,
                                     const std::vector<double>& weights,
                                     const std::uint64_t dead_mask) {
    if (pair_indices.size() % 2 != 0) {
        throw std::invalid_argument("sparse range indices length must be even");
    }
    const std::size_t n = pair_indices.size() / 2;
    if (weights.size() != n) {
        throw std::invalid_argument("sparse range weights length must match index pairs");
    }
    SparseRange out;
    out.combos.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const int a = pair_indices[i * 2];
        const int b = pair_indices[i * 2 + 1];
        if (a < 0 || a > 51 || b < 0 || b > 51 || a == b) {
            throw std::invalid_argument("invalid deck index in sparse range");
        }
        const std::uint64_t m = dead_mask | (std::uint64_t{1} << a) | (std::uint64_t{1} << b);
        if ((dead_mask & (std::uint64_t{1} << a)) != 0 || (dead_mask & (std::uint64_t{1} << b)) != 0) {
            continue;
        }
        const double w = weights[i];
        if (!std::isfinite(w) || w <= 0.0) {
            continue;
        }
        WeightedHoleCombo c{};
        c.card_a = a;
        c.card_b = b;
        c.weight = w;
        out.combos.push_back(c);
        out.weight_sum += w;
    }
    if (out.combos.empty()) {
        throw std::invalid_argument("sparse range has no valid combos after blockers");
    }
    return out;
}

SparseRange sparse_range_from_dense1326(const double* weights, const std::size_t len,
                                        const std::uint64_t dead_mask) {
    if (len != 1326) {
        throw std::invalid_argument("dense range must have length 1326");
    }
    SparseRange out;
    int idx = 0;
    for (int a = 0; a < 52; ++a) {
        for (int b = a + 1; b < 52; ++b) {
            const double w = weights[static_cast<std::size_t>(idx++)];
            if (!std::isfinite(w) || w <= 0.0) {
                continue;
            }
            if ((dead_mask & (std::uint64_t{1} << a)) != 0 || (dead_mask & (std::uint64_t{1} << b)) != 0) {
                continue;
            }
            WeightedHoleCombo c{};
            c.card_a = a;
            c.card_b = b;
            c.weight = w;
            out.combos.push_back(c);
            out.weight_sum += w;
        }
    }
    if (out.combos.empty()) {
        throw std::invalid_argument("dense range has no valid combos after blockers");
    }
    return out;
}

}  // namespace poker
