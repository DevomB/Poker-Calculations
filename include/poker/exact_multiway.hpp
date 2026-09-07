#pragma once

#include "poker/card.hpp"

#include <cstdint>
#include <vector>

namespace poker {

struct MultiwayWinTieLose {
    std::vector<double> win;
    std::vector<double> split;
    std::vector<double> lose;
};

struct MultiwaySidePotChipEv {
    std::vector<double> chip_ev;
    int layer_count{0};
};

struct MultiwayAheadFrequency {
    double p_ahead_now{0.0};
    double p_win_showdown{0.0};
};

struct MultiwayTieFrequency {
    double p_hero_split{0.0};
    double p_any_split{0.0};
};

struct MultiwayBestWorstRunout {
    bool supported{false};
    Card best_card{};
    Card worst_card{};
    double best_equity{0.0};
    double worst_equity{0.0};
};

/**
 * Exact showdown equity for 3–6 known hole pairs. Board 0–5 cards; optional dead/muck.
 * Tie rule: each player tied for best receives `1 / tiedAtBest` (NUMERICAL.md).
 */
[[nodiscard]] std::vector<double> exact_multiway_equity_known_hands(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards = {});

[[nodiscard]] std::vector<double> exact_three_way_equity_known_hands(
    const std::vector<Card>& hole0, const std::vector<Card>& hole1, const std::vector<Card>& hole2,
    const std::vector<Card>& board_cards, const std::vector<Card>& dead_cards = {});

[[nodiscard]] std::vector<double> exact_four_way_equity_known_hands(
    const std::vector<Card>& hole0, const std::vector<Card>& hole1, const std::vector<Card>& hole2,
    const std::vector<Card>& hole3, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards = {});

/** win = unique best, split = tied for best, lose = rest. Frequencies per player sum to 1. */
[[nodiscard]] MultiwayWinTieLose exact_multiway_win_tie_lose_known_hands(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards = {});

[[nodiscard]] MultiwayWinTieLose exact_three_way_win_tie_lose_known_hands(
    const std::vector<Card>& hole0, const std::vector<Card>& hole1, const std::vector<Card>& hole2,
    const std::vector<Card>& board_cards, const std::vector<Card>& dead_cards = {});

[[nodiscard]] MultiwaySidePotChipEv exact_multiway_side_pot_chip_ev(
    const std::vector<double>& committed_chips, const std::vector<std::vector<Card>>& hole_hands,
    const std::vector<Card>& board_cards, const std::vector<Card>& dead_cards = {});

/** Flop or turn only. Hero is player 0. */
[[nodiscard]] MultiwayAheadFrequency exact_multiway_ahead_frequency(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards = {});

[[nodiscard]] MultiwayTieFrequency exact_multiway_tie_frequency(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards = {});

[[nodiscard]] std::uint64_t exact_multiway_runout_count(const std::vector<std::vector<Card>>& hole_hands,
                                                       const std::vector<Card>& board_cards,
                                                       const std::vector<Card>& dead_cards = {});

/** Next-street card only: turn when board is 3, river when board is 4. Else `supported = false`. */
[[nodiscard]] MultiwayBestWorstRunout exact_multiway_best_worst_runout(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards = {});

}  // namespace poker
