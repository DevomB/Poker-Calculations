#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace poker {

/**
 * Subtract posted blinds/antes, then ICM on remaining stacks. Hero posts `hero_post`;
 * every other seat posts `posts[i]`. Pot is dead for placement ICM (standard).
 */
[[nodiscard]] std::vector<double> icm_payouts_after_blind_post(const std::vector<double>& stacks,
                                                               const std::vector<double>& payouts,
                                                               std::size_t hero, double hero_post,
                                                               const std::vector<double>& posts);

/**
 * Calling bubble factor for one hero-vs-villain all-in:
 * BF = (EV_now - EV_lose) / (EV_win - EV_now) after transferring `chips_at_risk`.
 * Distinct from `icm_pairwise_bubble_factor`. Tiny gain → +∞ when there is a real dollar loss.
 */
[[nodiscard]] double icm_calling_bubble_factor(const std::vector<double>& stacks,
                                               const std::vector<double>& payouts, std::size_t hero,
                                               std::size_t villain, double chips_at_risk);

/// FGS over a blind schedule. Each level applies `orbits_at_level[i]` average-position orbits.
[[nodiscard]] std::vector<double> fgs_payouts_blind_schedule(const std::vector<double>& stacks,
                                                             const std::vector<double>& payouts,
                                                             const std::vector<double>& small_blinds,
                                                             const std::vector<double>& big_blinds,
                                                             const std::vector<double>& antes,
                                                             const std::vector<double>& orbits_at_level);

struct IcmDeadPotDollarEvResult {
    double now_ev{0.0};
    double win_ev{0.0};
    double delta{0.0};
};

/**
 * $EV of hero winning a dead pot of `dead_chips` (chips already in the middle, owned by nobody).
 * Placement is Harville on stacks; winning the pot is a two-point ICM: hero stack += dead_chips.
 */
[[nodiscard]] IcmDeadPotDollarEvResult icm_dead_pot_dollar_ev(const std::vector<double>& stacks,
                                                              const std::vector<double>& payouts,
                                                              std::size_t hero, double dead_chips);

}  // namespace poker
