#pragma once

namespace poker {

struct TournamentDuelResult {
    double hero_win_probability{0.0};
    double expected_hands{0.0};
    double hero_prize_ev{0.0};
};

[[nodiscard]] TournamentDuelResult tournament_duel_absorption_probabilities(
    double hero_stack, double villain_stack, double win_probability_per_hand,
    double chips_per_all_in, double winner_prize = 0.0);

}  // namespace poker
