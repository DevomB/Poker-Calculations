#pragma once

#include <vector>

namespace poker {

struct Side_pot_layer {
    double pot_chips{};
    std::vector<double> player_cap_contribution{};
};

/**
 * P20: from per-player total committed chips this hand, build main + side pot layers.
 * `committed[i]` is each player's total contribution to the pot (non-negative).
 */
[[nodiscard]] std::vector<Side_pot_layer> side_pot_ladder_from_commitments(
    const std::vector<double>& committed_chips);

/**
 * P21: chip EV per player from layer pot sizes and per-layer win probabilities (rows: player,
 * columns: layer index). Each column must sum to 1 (within tolerance).
 */
[[nodiscard]] std::vector<double> layered_pot_chip_ev_from_equities(
    const std::vector<double>& layer_pot_chips,
    const std::vector<std::vector<double>>& equity_player_by_layer);

}  // namespace poker
