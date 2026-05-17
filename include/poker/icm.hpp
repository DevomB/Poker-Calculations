#pragma once

#include <cstddef>
#include <vector>

namespace poker {

/**
 * P17: full Harville / Malmuth–Harville placement matrix.
 * `out[i][r]` = P(player index `i` finishes in place `r+1`), where `r=0` is first place.
 */
[[nodiscard]] std::vector<std::vector<double>> icm_harville_placement_probabilities(
    const std::vector<double>& stacks);

/// Harville first-place probabilities (column `r=0` of `icm_harville_placement_probabilities`).
[[nodiscard]] std::vector<double> icm_win_probabilities_harville(const std::vector<double>& stacks);

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

}  // namespace poker
