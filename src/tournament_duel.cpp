#include "poker/tournament_duel.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace poker {

TournamentDuelResult tournament_duel_absorption_probabilities(
    double hero_stack, double villain_stack, double win_probability_per_hand,
    double chips_per_all_in, double winner_prize) {
    if (hero_stack <= 0.0 || villain_stack <= 0.0) {
        throw std::invalid_argument("tournamentDuel: stacks must be positive");
    }
    if (chips_per_all_in <= 0.0) {
        throw std::invalid_argument("tournamentDuel: chipsPerAllIn must be positive");
    }
    if (win_probability_per_hand < 0.0 || win_probability_per_hand > 1.0) {
        throw std::invalid_argument("tournamentDuel: win probability must be in [0,1]");
    }
    // Gambler's ruin. Each all-in moves `chips_per_all_in` chips; hero busts after h losses and
    // wins after v net wins. Closed form (Feller), so stack asymmetry is respected exactly.
    const double h = std::max(1.0, std::ceil(hero_stack / chips_per_all_in));
    const double v = std::max(1.0, std::ceil(villain_stack / chips_per_all_in));
    const double p = win_probability_per_hand;
    const double q = 1.0 - p;

    TournamentDuelResult out;
    if (p <= 0.0) {
        out.hero_win_probability = 0.0;
        out.expected_hands = h;
    } else if (p >= 1.0) {
        out.hero_win_probability = 1.0;
        out.expected_hands = v;
    } else if (std::abs(p - q) < 1e-12) {
        out.hero_win_probability = h / (h + v);
        out.expected_hands = h * v;
    } else {
        const double r = q / p;
        const double rh = std::pow(r, h);
        const double rn = std::pow(r, h + v);
        out.hero_win_probability = (1.0 - rh) / (1.0 - rn);
        out.expected_hands = h / (q - p) - (h + v) / (q - p) * out.hero_win_probability;
    }
    out.hero_prize_ev = out.hero_win_probability * winner_prize;
    return out;
}

}  // namespace poker
