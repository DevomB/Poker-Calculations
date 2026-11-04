#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace poker {

inline constexpr int kHoleComboCount = 1326;

/// Dense 1326 slot of two distinct deck ids (0..51) in either order: lexicographic over a < b,
/// `a * 51 - a * (a - 1) / 2 + (b - a - 1)`, so (0,1) -> 0, (0,51) -> 50, (1,2) -> 51, (50,51) -> 1325.
/// Trusted form for ids the caller already validated; no bounds checks.
[[nodiscard]] constexpr int combo_index_1326_unchecked(int a, int b) noexcept {
    if (a > b) {
        std::swap(a, b);
    }
    return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

/// Checked form of `combo_index_1326_unchecked`: -1 unless `a` and `b` are distinct ids in 0..51.
[[nodiscard]] constexpr int combo_index_1326(int a, int b) noexcept {
    if (a == b || a < 0 || b < 0 || a > 51 || b > 51) {
        return -1;
    }
    return combo_index_1326_unchecked(a, b);
}

/// Inverse of `combo_index_1326`: the pair (a, b) with a < b, or {-1, -1} outside [0, 1326).
[[nodiscard]] constexpr std::pair<int, int> combo_cards_1326(int index) noexcept {
    if (index < 0 || index >= kHoleComboCount) {
        return {-1, -1};
    }
    for (int a = 0; a < 51; ++a) {
        const int width = 51 - a;
        if (index < width) {
            return {a, a + 1 + index};
        }
        index -= width;
    }
    return {-1, -1};
}

/// Throws std::invalid_argument unless `w` is a finite, non-negative range weight. A NaN, infinite,
/// or negative weight has no distribution to represent, so it is rejected rather than dropped.
inline void require_range_weight(double w) {
    if (!std::isfinite(w) || w < 0.0) {
        throw std::invalid_argument("range weights must be finite and non-negative");
    }
}

/**
 * Only relative weights matter. When the largest weight lies outside [2^-256, 2^256], every weight
 * is multiplied by the same power of two (exact in binary floating point) so the largest lands in
 * [0.5, 1); otherwise nothing changes. Totals of any number of combos, their products with
 * probabilities and runout counts, and the pairwise products of two ranges summed over every pair
 * (the CFR subgames) then stay finite and normal: [1, 1] and [1e308, 1e308] are the same
 * distribution. Validates every weight first. Returns the total of the (possibly rescaled) weights.
 */
template <typename Combo>
double rescale_range_weights(std::vector<Combo>& combos, int* shift_out = nullptr) {
    double max_w = 0.0;
    for (const Combo& c : combos) {
        require_range_weight(c.weight);
        max_w = std::max(max_w, c.weight);
    }
    int shift = 0;
    if (max_w > 0.0 && (max_w > 0x1p256 || max_w < 0x1p-256)) {
        int e = 0;
        (void)std::frexp(max_w, &e);  // max_w = f * 2^e with f in [0.5, 1)
        shift = -e;
    }
    double total = 0.0;
    for (Combo& c : combos) {
        if (shift != 0) {
            c.weight = std::ldexp(c.weight, shift);
        }
        total += c.weight;
    }
    if (shift_out) {
        *shift_out = shift;
    }
    return total;
}

/// Villain hole as two deck indices (0..51) with non-negative weight.
struct WeightedHoleCombo {
    int card_a{0};
    int card_b{0};
    double weight{0.0};
};

/// Sparse villain range: combos with positive weights (blockers removed by the builder). Builders
/// apply `rescale_range_weights`, so `weight_sum` is always finite.
struct SparseRange {
    std::vector<WeightedHoleCombo> combos;
    double weight_sum{0.0};
    /// The input weights were multiplied by 2^weight_shift; `std::ldexp(x, -weight_shift)` turns a
    /// weighted total back into input units (0 for ordinary ranges).
    int weight_shift{0};
};

/// Build sparse range from parallel `indices` (2*n) and `weights` (n), excluding `dead` bitset.
[[nodiscard]] SparseRange sparse_range_from_arrays(const std::vector<int>& pair_indices,
                                                   const std::vector<double>& weights,
                                                   std::uint64_t dead_mask);

/// Build from dense length-1326 weights indexed by `combo_index_1326`.
[[nodiscard]] SparseRange sparse_range_from_dense1326(const double* weights, std::size_t len,
                                                      std::uint64_t dead_mask);

}  // namespace poker
