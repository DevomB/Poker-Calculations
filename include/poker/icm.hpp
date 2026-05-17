#pragma once

#include <cstddef>
#include <vector>

namespace poker {

/// Harville / Malmuth–Harville placement probabilities: `placement[i]` = P(player i wins).
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
