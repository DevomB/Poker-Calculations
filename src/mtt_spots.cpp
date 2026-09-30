#include "poker/mtt_spots.hpp"

#include "poker/icm.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace poker {
namespace {

void require_finite_nonneg(double x, const char* name) {
    if (!std::isfinite(x) || x < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and non-negative");
    }
}

void require_positive(double x, const char* name) {
    if (!std::isfinite(x) || x <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and positive");
    }
}

void require_unit(double x, const char* name) {
    if (!std::isfinite(x) || x < 0.0 || x > 1.0) {
        throw std::invalid_argument(std::string(name) + " must be in [0,1]");
    }
}

void apply_fgs_orbits(std::vector<double>& stacks, int orbits, double small_blind, double big_blind,
                      double ante) {
    if (orbits < 0) {
        throw std::invalid_argument("pkoFgsPayouts: orbits must be >= 0");
    }
    require_finite_nonneg(small_blind, "smallBlind");
    require_finite_nonneg(big_blind, "bigBlind");
    require_finite_nonneg(ante, "ante");
    const double cost = orbit_cost_chips(small_blind, big_blind, std::vector<double>{ante});
    for (int o = 0; o < orbits; ++o) {
        for (double& s : stacks) {
            if (s > 0.0) {
                s = std::max(0.0, s - cost);
            }
        }
    }
}

[[nodiscard]] double showdown_chip_ev(double equity, double pot, double hero_put, double villain_put) {
    return equity * (pot + hero_put + villain_put) - hero_put;
}

}  // namespace

std::vector<double> spin_go_payouts(double multiplier, double buyin, bool winner_take_all) {
    require_positive(multiplier, "multiplier");
    require_finite_nonneg(buyin, "buyin");
    const double pool = multiplier * buyin;
    if (winner_take_all) {
        return {pool, 0.0, 0.0};
    }
    return {0.5 * pool, 0.3 * pool, 0.2 * pool};
}

std::vector<double> spin_go_icm_ev(const std::vector<double>& stacks,
                                   const std::vector<double>& payouts) {
    if (stacks.size() != 3 || payouts.size() != 3) {
        throw std::invalid_argument("spinGoIcmEv: stacks and payouts must both have length 3");
    }
    return icm_expected_payouts(stacks, payouts);
}

PkoIcmbuResult pko_fgs_payouts(const std::vector<double>& stacks, const std::vector<double>& payouts,
                               const std::vector<double>& bounty_values, int orbits,
                               double small_blind, double big_blind, double ante) {
    auto surviving = stacks;
    apply_fgs_orbits(surviving, orbits, small_blind, big_blind, ante);
    return pko_icmbu_payouts(surviving, payouts, bounty_values);
}

SpotChipEv squeeze_ev(double pot, double hero_put, double opener_call, double caller_call,
                      double fold_equity_opener, double fold_equity_caller, double equity_vs_opener,
                      double equity_vs_caller, double equity_vs_both) {
    require_finite_nonneg(pot, "pot");
    require_finite_nonneg(hero_put, "heroPut");
    require_finite_nonneg(opener_call, "openerCall");
    require_finite_nonneg(caller_call, "callerCall");
    require_unit(fold_equity_opener, "foldEquityOpener");
    require_unit(fold_equity_caller, "foldEquityCaller");
    require_unit(equity_vs_opener, "equityVsOpener");
    require_unit(equity_vs_caller, "equityVsCaller");
    require_unit(equity_vs_both, "equityVsBoth");

    const double p_both_fold = fold_equity_opener * fold_equity_caller;
    const double p_opener_only = (1.0 - fold_equity_opener) * fold_equity_caller;
    const double p_caller_only = fold_equity_opener * (1.0 - fold_equity_caller);
    const double p_both = (1.0 - fold_equity_opener) * (1.0 - fold_equity_caller);

    SpotChipEv r;
    r.fold_ev = 0.0;
    r.take_ev = p_both_fold * pot +
                p_opener_only * showdown_chip_ev(equity_vs_opener, pot, hero_put, opener_call) +
                p_caller_only * showdown_chip_ev(equity_vs_caller, pot, hero_put, caller_call) +
                p_both * showdown_chip_ev(equity_vs_both, pot, hero_put, opener_call + caller_call);
    r.delta = r.take_ev - r.fold_ev;
    return r;
}

SpotChipEv four_bet_jam_ev(double dead_pot, double jam, double call, double fold_equity,
                           double equity_when_called) {
    require_finite_nonneg(dead_pot, "deadPot");
    require_finite_nonneg(jam, "jam");
    require_finite_nonneg(call, "call");
    require_unit(fold_equity, "foldEquity");
    require_unit(equity_when_called, "equityWhenCalled");

    SpotChipEv r;
    r.fold_ev = 0.0;
    r.take_ev = fold_equity * dead_pot +
                (1.0 - fold_equity) * showdown_chip_ev(equity_when_called, dead_pot, jam, call);
    r.delta = r.take_ev - r.fold_ev;
    return r;
}

ThreeBetCommitEv three_bet_pot_commit_ev(double pot_after_three_bet, double effective_remaining,
                                         double equity, double realization) {
    require_finite_nonneg(pot_after_three_bet, "potAfterThreeBet");
    require_finite_nonneg(effective_remaining, "effectiveRemaining");
    require_unit(equity, "equity");
    require_unit(realization, "realization");

    ThreeBetCommitEv r;
    r.spr = spr(pot_after_three_bet, effective_remaining);
    const double realized = std::min(1.0, std::max(0.0, equity * realization));
    const double final_pot = pot_after_three_bet + 2.0 * effective_remaining;
    const double pot_odds = final_pot > 0.0 ? effective_remaining / final_pot : 0.0;
    r.stack_off = realized + 1e-15 >= pot_odds;
    r.continue_ev = realized * final_pot - effective_remaining;
    r.fold_ev = 0.0;
    return r;
}

}  // namespace poker
