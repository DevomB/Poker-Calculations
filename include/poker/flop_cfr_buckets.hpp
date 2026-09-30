#pragma once

#include "poker/card.hpp"
#include "poker/hand_potential.hpp"
#include "poker/range.hpp"

#include <string>
#include <vector>

namespace poker {

/// Default EHS2 bucket count for flop abstractions (equal-width on [0, 1]).
inline constexpr int kDefaultFlopBucketCount = 20;

[[nodiscard]] int flop_bucket_count_default();

/**
 * Map every 1326 hero combo to an EHS2 bucket vs `villain_range` on this flop (or turn).
 * Live combos are in `[0, k)`. Combos blocked by the board are `-1`.
 */
[[nodiscard]] std::vector<int> ehs2_buckets_vs_range(const std::vector<Card>& board_cards,
                                                     const SparseRange& villain_range,
                                                     int bucket_count = kDefaultFlopBucketCount,
                                                     const ComboEhsTableOptions& options = {});

/**
 * Normalize a 1326 range into `k` bucket masses (sum ≈ 1). Blocked / negative bucket ids
 * contribute nothing.
 */
[[nodiscard]] std::vector<double> bucket_mass_from_range(const std::vector<double>& range_1326,
                                                         const std::vector<int>& combo_buckets,
                                                         int bucket_count);

[[nodiscard]] std::vector<double> bucket_mass_from_range(const SparseRange& range,
                                                         const std::vector<int>& combo_buckets,
                                                         int bucket_count);

/// Copy each bucket's mixed action onto every 1326 combo in that bucket (0 if blocked).
[[nodiscard]] std::vector<double> flop_bucket_strategy_to_1326(const std::vector<double>& bucket_mix,
                                                               const std::vector<int>& combo_buckets);

/**
 * Cache key: canonical flop index (0..1754 of 1755) plus quantized pot/stack.
 * Suit-isomorphic flops share the same key.
 */
[[nodiscard]] std::string canonical_flop_cfr_key(const std::vector<Card>& flop, double pot,
                                                 double stack = 0.0);

}  // namespace poker
