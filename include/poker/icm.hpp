#pragma once

#include <cstddef>
#include <vector>

namespace poker {

/// Full placement matrices are computed for up to this many players (all 2^n subsets).
inline constexpr std::size_t kHarvilleDenseMaxPlayers = 20;
/// Above that, the number of subset states needed for the places asked for must stay within this.
inline constexpr std::size_t kHarvilleMaxStates = std::size_t{1} << 20;

/**
 * Harville placement probabilities for the first `places` places from first-place weights:
 * `out[i][j]` = P(player `i` finishes in place `j + 1`). Each place goes to a remaining player with
 * probability proportional to their weight (scaled over the remaining players only); players whose
 * weights are all 0 among the remaining finish in a uniformly random order. Computed over subsets
 * of already-placed players, not orderings; throws when that needs more than `kHarvilleMaxStates`
 * states for a field larger than `kHarvilleDenseMaxPlayers`.
 */
[[nodiscard]] std::vector<std::vector<double>> harville_top_places(const std::vector<double>& weights,
                                                                   std::size_t places);

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

/// Same as `icm_expected_payouts`, but zero stacks are allowed: those seats have busted and split
/// the bottom prizes, and the rest play for the remaining places. Prize pool is conserved.
[[nodiscard]] std::vector<double> icm_expected_payouts_allowing_busts(const std::vector<double>& stacks,
                                                                    const std::vector<double>& payouts);

/**
 * Pairwise bubble factor: marginal $EV loss from losing `pot_chips` to villain j vs marginal gain
 * from winning the same pot (finite differences on `icm_expected_payouts`).
 */
[[nodiscard]] double icm_pairwise_bubble_factor(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts, std::size_t hero,
                                                std::size_t villain, double pot_chips);

/**
 * Deprecated power-utility allocator, not a payout model: the whole prize pool is split in proportion
 * to `stack_i^alpha` (default `alpha = 2`), ignoring finishing positions, so guaranteed prizes are not
 * respected. Use `icm_expected_payouts`.
 */
[[nodiscard]] std::vector<double> icm_expected_payouts_weitzman(const std::vector<double>& stacks,
                                                                const std::vector<double>& payouts,
                                                                double alpha = 2.0);

/**
 * Harville placement with tilted first-place weights
 * w_i = (1-blend)*(s_i/sum) + blend*skill_i (skill normalized to sum 1).
 */
[[nodiscard]] std::vector<double> icm_harville_skill_adjusted_payouts(
    const std::vector<double>& stacks, const std::vector<double>& payouts,
    const std::vector<double>& skill_weights, double blend);

[[nodiscard]] std::vector<double> icm_equal_chop_payouts(const std::vector<double>& payouts);

[[nodiscard]] std::vector<double> icm_chop_surplus_vs_equal_split(const std::vector<double>& stacks,
                                                                  const std::vector<double>& payouts);

[[nodiscard]] double icm_total_prize_pool(const std::vector<double>& payouts);

[[nodiscard]] std::vector<double> icm_deal_ev_per_chip(const std::vector<double>& stacks,
                                                     const std::vector<double>& payouts);

[[nodiscard]] std::vector<double> icm_satellite_advance_probability(const std::vector<double>& stacks,
                                                                    int paid_places);

[[nodiscard]] double icm_payout_structure_gini(const std::vector<double>& payouts);

[[nodiscard]] double icm_chip_leader_premium_vs_equal_chop(const std::vector<double>& stacks,
                                                           const std::vector<double>& payouts);

[[nodiscard]] std::vector<double> icm_expected_payouts_delta_from_chip_chop(
    const std::vector<double>& stacks, const std::vector<double>& payouts);

}  // namespace poker
