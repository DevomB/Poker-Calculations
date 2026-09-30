#pragma once

namespace poker {

struct RiverIndifferenceResult {
    double bet_size{0.0};
    double bluff_frequency{0.0};
    double defender_mdf{0.0};
    double ev_at_indifference{0.0};
};

[[nodiscard]] RiverIndifferenceResult solve_river_polarized_indifference_bet(
    double pot_before_bet, double num_value_combos, double num_bluff_combos, double mdf = -1.0);

struct PushFoldThresholdResult {
    double threshold_equity{0.0};
    double jam_ev_at_threshold{0.0};
};

[[nodiscard]] PushFoldThresholdResult solve_symmetric_push_fold_threshold(
    double effective_stack, double small_blind, double big_blind, double ante_per_player);

}  // namespace poker
