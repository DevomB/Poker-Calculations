#pragma once

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace poker {

/**
 * Kansas City 2-7 single draw (Ace high; straights and flushes count against you).
 *
 * Best hand is 7-5-4-3-2 rainbow. A-5-4-3-2 is Ace-high, not a wheel. Compare five-card
 * lows by inverting Ace-high no-wheel poker rank: lower packed strength is better.
 * After the draw a player discards 0–5 and replaces from the remaining deck.
 */

/// Ace-high 5-card eval with no wheel (A2345 is Ace-high, not a 5-straight).
[[nodiscard]] HandEvaluation evaluate_deuce_seven_five(const std::vector<Card>& five);

/// Packed strength; **lower is better** 2-7. Same bit layout as `pack_hand_strength`.
[[nodiscard]] std::uint64_t evaluate_deuce_seven_hand(const std::vector<Card>& five);

/// `nuts` / `smooth` / `rough` / `number` / `paired` / `twoPair` / `trips` / `straight` /
/// `flush` / `fullHouse` / `quads` / `straightFlush`.
[[nodiscard]] const char* evaluate_deuce_seven_category(const std::vector<Card>& five);

/// Unpaired, no straight, no flush, high card ≤ 8 (default) or ≤ 7 when `eight_pat` is false.
[[nodiscard]] bool deuce_seven_is_pat(const std::vector<Card>& five, bool eight_pat = true);

/// 7-5-4-3-2 unpaired unsuited.
[[nodiscard]] bool deuce_seven_nuts_pat(const std::vector<Card>& five);

struct DeuceSevenDrawSpec {
    std::vector<Card> discard_cards;
    std::vector<Card> keep_cards;
    int discard_count{0};
    bool has_discard_cards{false};
    bool has_keep_cards{false};
    bool has_count{false};
};

/// Hero equity (ties 0.5) after both stand or both draw. Exact when the replacement
/// tree is small; otherwise Monte Carlo with `trials` / `rng`.
[[nodiscard]] double deuce_seven_draw_equity_vs_known(const std::vector<Card>& hero,
                                                      const std::vector<Card>& villain,
                                                      const DeuceSevenDrawSpec& hero_draw,
                                                      const DeuceSevenDrawSpec& villain_draw,
                                                      const std::vector<Card>& extra_dead, int trials,
                                                      std::mt19937& rng);

struct DeuceSevenRoughSmooth {
    int cmp{0};
    bool a_smooth{false};
    bool b_smooth{false};
};

/// Both hands must be unpaired 8-high (no straight, no flush). `cmp` is -1 if `a` is
/// better 2-7, 1 if `b` is, 0 tie. Smooth = second card is not a 7.
[[nodiscard]] DeuceSevenRoughSmooth deuce_seven_rough_vs_smooth(const std::vector<Card>& a,
                                                               const std::vector<Card>& b);

/// Pot-share vector (ties split). Hands must be disjoint 5-card holdings.
[[nodiscard]] std::vector<double> deuce_seven_multiway_showdown(
    const std::vector<std::vector<Card>>& hands);

}  // namespace poker
