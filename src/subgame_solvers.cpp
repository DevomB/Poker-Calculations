#include "poker/subgame_solvers.hpp"

#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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
    const double v_share = num_value_combos / n;
    // Villain's bluff-catch EV at bet b when the whole range bets: (1 - v)(P + b) - v b.
    // It is zero when the bluff share equals b / (P + 2b), i.e. b* = (1 - v) P / (2v - 1),
    // which needs a value-heavy range (v > 1/2). Below that, any size is a profitable call.
    if (v_share <= 0.5) {
        out.bet_size = std::numeric_limits<double>::infinity();
        out.bluff_frequency = 1.0;
        out.defender_mdf = mdf >= 0.0 ? mdf : 0.0;
        out.ev_at_indifference = (1.0 - v_share) * pot_before_bet;
        return out;
    }
    out.bet_size = (1.0 - v_share) * pot_before_bet / (2.0 * v_share - 1.0);
    out.defender_mdf = mdf >= 0.0 ? mdf : minimum_defense_frequency(pot_before_bet, out.bet_size);
    // Bluff combos needed so the betting range is exactly b / (P + 2b) bluffs; at b* this is
    // the whole bluff supply, so the frequency is 1 unless the caller passes a larger supply.
    const double bluffs_needed = num_value_combos * out.bet_size / (pot_before_bet + out.bet_size);
    out.bluff_frequency =
        num_bluff_combos > 0.0 ? std::clamp(bluffs_needed / num_bluff_combos, 0.0, 1.0) : 0.0;
    out.ev_at_indifference =
        (1.0 - v_share) * (pot_before_bet + out.bet_size) - v_share * out.bet_size;
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

}  // namespace poker
