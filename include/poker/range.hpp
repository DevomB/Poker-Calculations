#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace poker {

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

/// Build from dense length-1326 weights (index = c1*51 + c2 with c1 < c2 canonical).
[[nodiscard]] SparseRange sparse_range_from_dense1326(const double* weights, std::size_t len,
                                                      std::uint64_t dead_mask);

}  // namespace poker
