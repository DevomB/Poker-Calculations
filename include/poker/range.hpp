#pragma once

#include <cstddef>
#include <cstdint>
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

/// Villain hole as two deck indices (0..51) with non-negative weight.
struct WeightedHoleCombo {
    int card_a{0};
    int card_b{0};
    double weight{0.0};
};

/// Sparse villain range: sorted unique combos with weights (blockers removed by builder).
struct SparseRange {
    std::vector<WeightedHoleCombo> combos;
    double weight_sum{0.0};
};

/// Build sparse range from parallel `indices` (2*n) and `weights` (n), excluding `dead` bitset.
[[nodiscard]] SparseRange sparse_range_from_arrays(const std::vector<int>& pair_indices,
                                                   const std::vector<double>& weights,
                                                   std::uint64_t dead_mask);

/// Build from dense length-1326 weights indexed by `combo_index_1326`.
[[nodiscard]] SparseRange sparse_range_from_dense1326(const double* weights, std::size_t len,
                                                      std::uint64_t dead_mask);

}  // namespace poker
