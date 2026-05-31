#pragma once

#include "poker/card.hpp"

#include <vector>

namespace poker {

struct RiverIndifferenceResult {
    double bet_size{0.0};
    double bluff_frequency{0.0};
    double defender_mdf{0.0};
    double ev_at_indifference{0.0};
};

[[nodiscard]] RiverIndifferenceResult solve_river_polarized_indifference_bet(
    double pot_before_bet, double num_value_combos, double num_bluff_combos, double mdf = -1.0);

struct StageMinimaxRegretResult {
    double best_bet{0.0};
    double minimax_regret{0.0};
    std::vector<double> ev_by_action;
};

[[nodiscard]] StageMinimaxRegretResult solve_stage_minimax_regret_bet(
    double pot_before_bet, const std::vector<double>& bet_sizes, double villain_fold_freq,
    double villain_call_freq, double hero_equity_when_called);

struct PushFoldThresholdResult {
    double threshold_equity{0.0};
    double jam_ev_at_threshold{0.0};
};

[[nodiscard]] PushFoldThresholdResult solve_symmetric_push_fold_threshold(
    double effective_stack, double small_blind, double big_blind, double ante_per_player);

struct MultiwayIndependenceGapResult {
    double exact{0.0};
    double independent_approx{0.0};
    double gap{0.0};
    int villains{0};
};

[[nodiscard]] MultiwayIndependenceGapResult multiway_equity_independence_gap(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    int num_simulations, int seed, int villains);

}  // namespace poker
