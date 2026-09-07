#pragma once

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace poker {

/// Sparse PLO range: each combo is four deck ids (0..51) plus a weight.
struct OmahaRangeCombo {
    int cards[4]{};
    double weight{1.0};
};

/// Flop wrap/OESD outs under Omaha 2-hole + 3-board.
struct OmahaWrapDrawOuts {
    int outs{0};
    int nut_outs{0};
};

/// Best 5-card Omaha hand: exactly 2 hole + exactly 3 board. `hole` is 4 cards; `board` is 3–5.
[[nodiscard]] HandEvaluation evaluate_omaha_best_hand(const std::vector<Card>& hole,
                                                      const std::vector<Card>& board);

/// Same `pack_hand_strength` encoding as Hold'em 5-card strength (not 7-card best-of-9).
[[nodiscard]] std::uint64_t evaluate_omaha_hand_strength(const std::vector<Card>& hole,
                                                         const std::vector<Card>& board);

/// Exact HU equity vs a known 4-card hand. Board 0–5; remaining runouts enumerated.
[[nodiscard]] double exact_hu_omaha_equity_vs_known(const std::vector<Card>& hero,
                                                    const std::vector<Card>& villain,
                                                    const std::vector<Card>& board);

[[nodiscard]] float simulate_omaha_equity_vs_random(const std::vector<Card>& hero,
                                                    const std::vector<Card>& board, int trials,
                                                    std::mt19937& rng);

[[nodiscard]] float simulate_omaha_equity_vs_range(const std::vector<Card>& hero,
                                                   const std::vector<Card>& board,
                                                   const std::vector<OmahaRangeCombo>& range,
                                                   int trials, std::mt19937& rng);

/// `C(52 − |dead|, 4)` remaining 4-card combos after unique dead cards.
[[nodiscard]] std::uint64_t omaha_combo_count(const std::vector<Card>& dead);

[[nodiscard]] bool omaha_nuts_on_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                                       const std::vector<Card>& extra_dead = {});

/// Next-street cards that make a straight (or SF/royal) via 2+3. Flop only (`board` length 3).
[[nodiscard]] OmahaWrapDrawOuts omaha_wrap_draw_outs(const std::vector<Card>& hero,
                                                     const std::vector<Card>& flop);

/// 0–1: `1 − (# strictly better 4-card holdings) / (n − 1)` on this board. Unique nuts → 1.
[[nodiscard]] double omaha_nuttedness_score(const std::vector<Card>& hero,
                                            const std::vector<Card>& board,
                                            const std::vector<Card>& extra_dead = {});

/// MC pot-share equity for 3–4 known 4-card hands. Length matches `holes`.
[[nodiscard]] std::vector<double> omaha_multiway_equity_mc(
    const std::vector<std::vector<Card>>& holes, const std::vector<Card>& board, int trials,
    std::mt19937& rng);

}  // namespace poker
