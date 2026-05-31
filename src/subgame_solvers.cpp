#include "poker/subgame_solvers.hpp"

#include "poker/monte_carlo.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace poker {

RiverIndifferenceResult solve_river_polarized_indifference_bet(double pot_before_bet,
                                                              double num_value_combos,
                                                              double num_bluff_combos,
                                                              double mdf) {
    if (pot_before_bet < 0.0 || num_value_combos < 0.0 || num_bluff_combos < 0.0) {
        throw std::invalid_argument("solveRiverPolarizedIndifferenceBet: invalid inputs");
    }
    const double n = num_value_combos + num_bluff_combos;
    if (n <= 0.0) {
        throw std::invalid_argument("solveRiverPolarizedIndifferenceBet: need positive combos");
    }
    RiverIndifferenceResult out;
    out.defender_mdf =
        mdf >= 0.0 ? mdf : minimum_defense_frequency(pot_before_bet, pot_before_bet);
    const double alpha = 1.0 - out.defender_mdf;
    const double v_share = num_value_combos / n;
    auto ev_villain_call = [&](double b) {
        const double pot_after = pot_before_bet + 2.0 * b;
        return v_share * pot_after - b;
    };
    double lo = 0.0;
    double hi = std::max(pot_before_bet, 1.0);
    for (int i = 0; i < 80; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (ev_villain_call(mid) > 0.0) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    out.bet_size = 0.5 * (lo + hi);
    out.bluff_frequency = alpha * (pot_before_bet + out.bet_size) /
                          std::max(out.bet_size, 1e-9) / std::max(num_bluff_combos / n, 1e-9);
    out.bluff_frequency = std::clamp(out.bluff_frequency, 0.0, 1.0);
    out.ev_at_indifference = ev_villain_call(out.bet_size);
    return out;
}

StageMinimaxRegretResult solve_stage_minimax_regret_bet(double pot_before_bet,
                                                        const std::vector<double>& bet_sizes,
                                                        double villain_fold_freq,
                                                        double villain_call_freq, double hero_equity) {
    if (pot_before_bet < 0.0) {
        throw std::invalid_argument("solveStageMinimaxRegretBet: pot must be non-negative");
    }
    villain_fold_freq = std::clamp(villain_fold_freq, 0.0, 1.0);
    villain_call_freq = std::clamp(villain_call_freq, 0.0, 1.0);
    const double respond = villain_fold_freq + villain_call_freq;
    const double fold_p = respond > 0.0 ? villain_fold_freq / respond : 0.5;
    const double call_p = respond > 0.0 ? villain_call_freq / respond : 0.5;

    auto ev_bet = [&](double b) {
        const double pot_if_fold = pot_before_bet + b;
        const double pot_if_call = pot_before_bet + 2.0 * b;
        return fold_p * pot_if_fold + call_p * (hero_equity * pot_if_call - b);
    };

    StageMinimaxRegretResult out;
    double best_ev = ev_bet(0.0);
    out.best_bet = 0.0;
    out.ev_by_action.push_back(best_ev);
    for (double b : bet_sizes) {
        const double ev = ev_bet(b);
        out.ev_by_action.push_back(ev);
        if (ev > best_ev) {
            best_ev = ev;
            out.best_bet = b;
        }
    }
    out.minimax_regret = best_ev - out.ev_by_action.front();
    return out;
}

PushFoldThresholdResult solve_symmetric_push_fold_threshold(double effective_stack,
                                                            double small_blind, double big_blind,
                                                            double ante_per_player) {
    if (effective_stack <= 0.0) {
        throw std::invalid_argument("solveSymmetricPushFoldThreshold: stack must be positive");
    }
    const double dead = small_blind + big_blind + 2.0 * ante_per_player;
  auto jam_ev = [&](double eq) {
        return eq * (2.0 * effective_stack + dead) - effective_stack;
    };
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < 64; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (jam_ev(mid) >= 0.0) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    PushFoldThresholdResult out;
    out.threshold_equity = 0.5 * (lo + hi);
    out.jam_ev_at_threshold = jam_ev(out.threshold_equity);
    return out;
}

MultiwayIndependenceGapResult multiway_equity_independence_gap(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    int num_simulations, int seed, int villains) {
    if (villains < 1) {
        throw std::invalid_argument("multiwayEquityIndependenceGap: villains must be >= 1");
    }
    MultiwayIndependenceGapResult out;
    out.villains = villains;
    std::mt19937 rng(static_cast<std::uint32_t>(seed));
    out.exact = static_cast<double>(
        simulate_hand_outcome(hero_hole_cards, board_cards, num_simulations, rng, villains));
    double prod = 1.0;
    for (int j = 0; j < villains; ++j) {
        std::mt19937 rng_hu(static_cast<std::uint32_t>(seed + 1000 + j));
        const double hu = static_cast<double>(simulate_hand_outcome(
            hero_hole_cards, board_cards, num_simulations, rng_hu, 1));
        prod *= (1.0 - hu);
    }
    out.independent_approx = 1.0 - prod;
    out.gap = out.exact - out.independent_approx;
    return out;
}

}  // namespace poker
