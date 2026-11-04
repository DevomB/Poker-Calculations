#include "poker/range.hpp"

#include "poker/deck_bitset.hpp"

#include <array>
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
    std::array<int, kHoleComboCount> slot{};  // 1 + position in out.combos, 0 = not seen yet
    for (std::size_t i = 0; i < n; ++i) {
        const int a = pair_indices[i * 2];
        const int b = pair_indices[i * 2 + 1];
        if (a < 0 || a > 51 || b < 0 || b > 51 || a == b) {
            throw std::invalid_argument("invalid deck index in sparse range");
        }
        const double w = weights[i];
        require_range_weight(w);
        if ((dead_mask & (std::uint64_t{1} << a)) != 0 || (dead_mask & (std::uint64_t{1} << b)) != 0 ||
            w == 0.0) {
            continue;
        }
        int& pos = slot[static_cast<std::size_t>(combo_index_1326_unchecked(a, b))];
        if (pos != 0) {
            // A combo listed more than once is one combo whose weight is the sum of its entries.
            double& merged = out.combos[static_cast<std::size_t>(pos - 1)].weight;
            merged += w;
            if (!std::isfinite(merged)) {
                throw std::invalid_argument(
                    "range weights are too large: a combo listed more than once sums past the largest double");
            }
            continue;
        }
        out.combos.push_back(WeightedHoleCombo{a, b, w});
        pos = static_cast<int>(out.combos.size());
    }
    for (const WeightedHoleCombo& c : out.combos) {
        out.weight_sum += c.weight;
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
            require_range_weight(w);
            if (w == 0.0 || (dead_mask & (std::uint64_t{1} << a)) != 0 ||
                (dead_mask & (std::uint64_t{1} << b)) != 0) {
                continue;
            }
            out.combos.push_back(WeightedHoleCombo{a, b, w});
            out.weight_sum += w;
        }
    }
    if (out.combos.empty()) {
        throw std::invalid_argument("dense range has no valid combos after blockers");
    }
    return out;
}

}  // namespace poker
