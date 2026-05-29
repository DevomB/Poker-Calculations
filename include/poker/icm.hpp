#pragma once

#include <cstddef>
#include <vector>

namespace poker {

/**
 * full Harville / Malmuth–Harville placement matrix.
 * `out[i][r]` = P(player index `i` finishes in place `r+1`), where `r=0` is first place.
 */
[[nodiscard]] std::vector<std::vector<double>> icm_harville_placement_probabilities(
    const std::vector<double>& stacks);

/// Harville first-place probabilities (column `r=0` of `icm_harville_placement_probabilities`).
[[nodiscard]] std::vector<double> icm_win_probabilities_harville(const std::vector<double>& stacks);

/**
 * Per-player probability of finishing in **one of the first `k` places** under Harville placement
 * (sum of first `k` columns of `icm_harville_placement_probabilities`). `k` in `1..n`.
 */
[[nodiscard]] std::vector<double> icm_top_k_finish_probabilities(const std::vector<double>& stacks, int k);

/// Harville probability each player finishes **last** (column `n-1` of the placement matrix).
[[nodiscard]] std::vector<double> icm_last_place_probabilities_harville(const std::vector<double>& stacks);

/// Expected payout chips (or dollars) per seat for `payouts[0]` = first prize, etc.
[[nodiscard]] std::vector<double> icm_expected_payouts(const std::vector<double>& stacks,
                                                       const std::vector<double>& payouts);

/**
 * Pairwise bubble factor: marginal $EV loss from losing `pot_chips` to villain j vs marginal gain
 * from winning the same pot (finite differences on `icm_expected_payouts`).
 */
[[nodiscard]] double icm_pairwise_bubble_factor(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts, std::size_t hero,
                                                std::size_t villain, double pot_chips);

/**
 * Independent Weitzman-style chip utility: each payout tier `k` is split among players in proportion to
 * `stack_i^alpha` (default `alpha = 2`). Simpler than Harville placement; useful as an alternative model.
 */
[[nodiscard]] std::vector<double> icm_expected_payouts_weitzman(const std::vector<double>& stacks,
                                                                const std::vector<double>& payouts,
                                                                double alpha = 2.0);

}  // namespace poker
