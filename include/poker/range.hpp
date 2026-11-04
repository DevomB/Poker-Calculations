#pragma once

#include <algorithm>
#include <array>
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
 * Only relative weights matter, so a calculation may multiply every weight it uses by one power of
 * two. `weight_scale_exponent(largest)` is the k for which `largest * 2^k` lies in [0.5, 1); scaling
 * by 2^k is exact, so ordinary inputs give bit-identical results, while totals of up to a few
 * thousand weights and their products with probabilities and runout counts stay finite. `largest`
 * must be the largest weight among the combos that actually take part in the calculation (after
 * blockers and card conflicts): a weight that is tiny next to that maximum contributes less than
 * 2^-1022 of the total, so its underflow cannot change the result, but scaling by a combo that later
 * drops out could zero every remaining weight. Returns 0 when `largest` is not positive and finite.
 */
[[nodiscard]] inline int weight_scale_exponent(double largest) noexcept {
    if (!(largest > 0.0) || !std::isfinite(largest)) {
        return 0;
    }
    int e = 0;
    (void)std::frexp(largest, &e);  // largest = f * 2^e with f in [0.5, 1)
    return -e;
}

/**
 * Accumulates `sum(a * x[k])` for each of N values and `sum(a)` over non-negative terms
 * `a = m * 2^e`, given as (m, e) so products of range weights such as 1e200 * 1e200, or a weight
 * times a small strategy probability, never overflow or underflow on the way in. Both sums are kept
 * relative to a power of two that tracks the largest term seen so far (exact rescaling), so a term
 * is lost only when it is below 2^-1074 of that largest term and cannot change any ratio
 * `num[k] / den`. For inputs whose raw products are representable the ratios are bit-identical to
 * plain accumulation.
 */
template <std::size_t N>
struct ScaledWeightedSums {
    std::array<double, N> num{};
    double den{0.0};
    int exp{0};
    bool any{false};

    void add(double m, int e, const std::array<double, N>& x) {
        if (!(m > 0.0)) {
            return;
        }
        int g = 0;
        m = std::frexp(m, &g);
        rebase(e + g);
        const double a = std::ldexp(m, e + g - exp);
        for (std::size_t k = 0; k < N; ++k) {
            num[k] += a * x[k];
        }
        den += a;
    }

    /// Adds another accumulator's totals (as if its terms had been added here, grouped).
    void merge(const ScaledWeightedSums& o) {
        if (!o.any) {
            return;
        }
        rebase(o.exp);
        for (std::size_t k = 0; k < N; ++k) {
            num[k] += std::ldexp(o.num[k], o.exp - exp);
        }
        den += std::ldexp(o.den, o.exp - exp);
    }

    [[nodiscard]] double ratio(std::size_t k = 0) const { return den > 0.0 ? num[k] / den : 0.0; }

private:
    void rebase(int e) {
        if (any && e <= exp) {
            return;
        }
        if (any) {
            for (double& v : num) {
                v = std::ldexp(v, exp - e);
            }
            den = std::ldexp(den, exp - e);
        }
        exp = e;
        any = true;
    }
};

/// Villain hole as two deck indices (0..51) with non-negative weight.
struct WeightedHoleCombo {
    int card_a{0};
    int card_b{0};
    double weight{0.0};
};

/// Sparse villain range: one entry per distinct combo with a positive weight, in the caller's units
/// (blockers removed by the builder; a combo listed twice is merged by adding its weights).
/// `weight_sum` is the total of those weights and is +infinity when it exceeds the largest double,
/// so calculations scale with `weight_scale_exponent` over their active combos instead of dividing
/// by it.
struct SparseRange {
    std::vector<WeightedHoleCombo> combos;
    double weight_sum{0.0};
};

/// Largest weight among `combos` (0 when empty).
template <typename Combo>
[[nodiscard]] double max_combo_weight(const std::vector<Combo>& combos) noexcept {
    double m = 0.0;
    for (const Combo& c : combos) {
        m = std::max(m, c.weight);
    }
    return m;
}

/// Build sparse range from parallel `indices` (2*n) and `weights` (n), excluding `dead` bitset.
[[nodiscard]] SparseRange sparse_range_from_arrays(const std::vector<int>& pair_indices,
                                                   const std::vector<double>& weights,
                                                   std::uint64_t dead_mask);

/// Build from dense length-1326 weights indexed by `combo_index_1326`.
[[nodiscard]] SparseRange sparse_range_from_dense1326(const double* weights, std::size_t len,
                                                      std::uint64_t dead_mask);

}  // namespace poker
