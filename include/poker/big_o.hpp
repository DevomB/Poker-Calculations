#pragma once

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/omaha.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace poker {

/// Sparse Big O range: each combo is five deck ids (0..51) plus a weight.
struct BigORangeCombo {
    int cards[5]{};
    double weight{1.0};
};

/// Best 5-card Big O hand: exactly 2 hole + exactly 3 board. `hole` is 5 cards; `board` is 3–5.
[[nodiscard]] HandEvaluation evaluate_big_o_best_hand(const std::vector<Card>& hole,
                                                      const std::vector<Card>& board);

/// Same `pack_hand_strength` encoding as Hold'em 5-card strength (not a 10-card best-of-N).
[[nodiscard]] std::uint64_t evaluate_big_o_hand_strength(const std::vector<Card>& hole,
                                                         const std::vector<Card>& board);

/// Exact HU equity vs a known 5-card hand. Board 0–5; remaining runouts enumerated.
[[nodiscard]] double exact_hu_big_o_equity_vs_known(const std::vector<Card>& hero,
                                                    const std::vector<Card>& villain,
                                                    const std::vector<Card>& board);

[[nodiscard]] float simulate_big_o_equity_vs_random(const std::vector<Card>& hero,
                                                    const std::vector<Card>& board, int trials,
                                                    std::mt19937& rng);

[[nodiscard]] float simulate_big_o_equity_vs_range(const std::vector<Card>& hero,
                                                   const std::vector<Card>& board,
                                                   const std::vector<BigORangeCombo>& range,
                                                   int trials, std::mt19937& rng);

/// `C(52 − |dead|, 5)` remaining 5-card combos after unique dead cards.
[[nodiscard]] std::uint64_t big_o_combo_count(const std::vector<Card>& dead);

[[nodiscard]] bool big_o_nuts_on_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                                       const std::vector<Card>& extra_dead = {});

/// MC pot-share equity for 3–4 known 5-card hands. Length matches `holes`.
[[nodiscard]] std::vector<double> big_o_multiway_equity_mc(
    const std::vector<std::vector<Card>>& holes, const std::vector<Card>& board, int trials,
    std::mt19937& rng);

}  // namespace poker
