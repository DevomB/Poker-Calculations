#include "poker/tournament_duel.hpp"

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
    const int max_steps =
        static_cast<int>(std::ceil((hero_stack + villain_stack) / chips_per_all_in)) + 2;
    const int states = 2 * max_steps + 1;
    const int offset = max_steps;
    const int hero_win_state = offset + max_steps;
    const int villain_win_state = offset - max_steps;

    std::vector<double> p(states, 0.0);
    std::vector<double> p_next(states, 0.0);
    std::vector<double> hands(states, 0.0);
    std::vector<double> hands_next(states, 0.0);
    p[offset] = 1.0;

    const double p_win = win_probability_per_hand;
    const double p_lose = 1.0 - p_win;

    for (int step = 0; step < max_steps * 4; ++step) {
        std::fill(p_next.begin(), p_next.end(), 0.0);
        std::fill(hands_next.begin(), hands_next.end(), 0.0);
        for (int d = -max_steps; d <= max_steps; ++d) {
            const int idx = offset + d;
            const double mass = p[idx];
            if (mass < 1e-15) {
                continue;
            }
            const int new_win = idx + 1;
            const int new_lose = idx - 1;
            if (new_win >= hero_win_state) {
                p_next[hero_win_state] += mass * p_win;
                hands_next[hero_win_state] += (hands[idx] + 1.0) * mass * p_win;
            } else {
                p_next[new_win] += mass * p_win;
                hands_next[new_win] += (hands[idx] + 1.0) * mass * p_win;
            }
            if (new_lose <= villain_win_state) {
                p_next[villain_win_state] += mass * p_lose;
                hands_next[villain_win_state] += (hands[idx] + 1.0) * mass * p_lose;
            } else {
                p_next[new_lose] += mass * p_lose;
                hands_next[new_lose] += (hands[idx] + 1.0) * mass * p_lose;
            }
        }
        p.swap(p_next);
        hands.swap(hands_next);
        if (p[hero_win_state] + p[villain_win_state] > 1.0 - 1e-12) {
            break;
        }
    }

    TournamentDuelResult out;
    out.hero_win_probability = p[hero_win_state];
    const double total_hands =
        hands[hero_win_state] + hands[villain_win_state] + 1e-300;
    out.expected_hands = total_hands;
    out.hero_prize_ev = out.hero_win_probability * winner_prize;
    return out;
}

}  // namespace poker
