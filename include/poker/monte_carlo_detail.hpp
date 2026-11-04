#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/range.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace poker {

struct McEquityDetailedResult {
    double estimate{0.0};
    double se{0.0};
    double ci_low{0.0};
    double ci_high{0.0};
    int n{0};
};

[[nodiscard]] McEquityDetailedResult simulate_hand_outcome_detailed(
    const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
    int num_simulations, std::mt19937& rng, int villains = 1,
    const CancelPredicate* cancel = nullptr);

struct McRangesEquityResult {
    McEquityDetailedResult equity;
    /// Opponent-hand tuples drawn; `equity.n / attempts` is the acceptance rate.
    std::int64_t attempts{0};
};

/**
 * Monte Carlo showdown equity of `player_hand` against one weighted range per opponent (1-9).
 * Combos that touch hero, the board, or `dead_cards` are skipped. Each trial draws a hand from
 * every range and keeps the tuple only if no two opponents share a card, so accepted tuples follow
 * P(h_1 .. h_k) proportional to w_1(h_1) ... w_k(h_k) over card-disjoint tuples; the board is then
 * completed uniformly. Hero's payoff is the showdown share (1/k on a k-way tie, no side pots), and
 * the uncertainty is reported as in `simulate_hand_outcome_detailed`. Throws std::invalid_argument
 * if a range has no live combo or fewer than `num_simulations` tuples are accepted within
 * `max_attempts` draws.
 */
[[nodiscard]] McRangesEquityResult simulate_equity_vs_ranges(
    const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
    const std::vector<SparseRange>& villain_ranges, const std::vector<Card>& dead_cards,
    int num_simulations, std::int64_t max_attempts, std::mt19937& rng,
    const CancelPredicate* cancel = nullptr);

[[nodiscard]] float simulate_equity_vs_range(const std::vector<Card>& player_hand,
                                             const std::vector<Card>& community_cards,
                                             const SparseRange& villain_range, int num_simulations,
                                             std::mt19937& rng, const CancelPredicate* cancel = nullptr);

}  // namespace poker
