#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"

#include <vector>

namespace poker {

/**
 * exact HU equity vs a uniformly random villain hand (two cards), board empty (preflop) or 3–5 cards.
 * Enumerates all villain combos and all completions of the board to five cards.
 */
[[nodiscard]] double exact_hu_equity_vs_random_hand(const std::vector<Card>& hero_hole_cards,
                                                  const std::vector<Card>& board_cards,
                                                  const CancelPredicate* should_cancel = nullptr);

/**
 * P(hero’s best 7 after turn+river is **straight or straight flush**), uniform over
 * all **unordered** two-card subsets of the remaining deck (hero 2 + flop 3 + optional `known_dead` known).
 * Equivalent to two cards dealt without replacement when only the final 7-card multiset matters.
 */
[[nodiscard]] double straight_made_flop_to_river_exact_probability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards,
    const CancelPredicate* should_cancel = nullptr);

/**
 * max jam stack (double, continuous) in `[0, max_stack_chips]` with nonnegative symmetric-jam EV,
 * using `exact_hu_equity_vs_random_hand` for equity.
 */
[[nodiscard]] double chubukov_max_symmetric_jam_stack_binary_search(const std::vector<Card>& hero_hole_cards,
                                                                   const std::vector<Card>& board_cards,
                                                                   double dead_money_chips, double max_stack_chips,
                                                                   int iterations);

/**
 * End-to-end: equity from `exact_hu_equity_vs_random_hand`, then largest integer jam stack in
 * `[1, max_stack_chips]` with nonnegative symmetric-jam toy EV (binary search on chips).
 */
[[nodiscard]] int chubukov_max_symmetric_jam_stack_from_hand_binary_search(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    double dead_money_chips, int max_stack_chips);

}  // namespace poker
