#include "poker/mtt_spots.hpp"

#include "poker/icm.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
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

SpinGoNashJamCallResult spin_go_nash_jam_call(double btn_stack, double sb_stack, double bb_stack,
                                              const std::vector<double>& payouts, double small_blind,
                                              double big_blind, double ante) {
    require_positive(btn_stack, "btnStack");
    require_positive(sb_stack, "sbStack");
    require_positive(bb_stack, "bbStack");
    if (payouts.size() != 3) {
        throw std::invalid_argument("spinGoNashJamCall: payouts must have length 3");
    }
    require_finite_nonneg(small_blind, "smallBlind");
    require_positive(big_blind, "bigBlind");
    require_finite_nonneg(ante, "ante");

    NashPushFoldSpec spec;
    spec.hero_stack = btn_stack;
    spec.hero_posted = 0.0;
    spec.villain_stack = sb_stack;
    spec.villain_posted = 0.0;
    spec.small_blind = small_blind;
    spec.big_blind = big_blind;
    spec.ante = ante;
    spec.use_icm = true;
    spec.payouts = payouts;

    const auto solved = nash_multiway_shove_call(spec, {sb_stack, bb_stack});
    SpinGoNashJamCallResult out;
    out.jam = solved.jam;
    out.iterations = solved.iterations;
    if (solved.calls.size() != 2) {
        throw std::logic_error("spinGoNashJamCall: expected two caller ranges");
    }
    out.sb_call = solved.calls[0];
    out.bb_call = solved.calls[1];
    return out;
}

PkoIcmbuResult pko_fgs_payouts(const std::vector<double>& stacks, const std::vector<double>& payouts,
                               const std::vector<double>& bounty_values, int orbits,
                               double small_blind, double big_blind, double ante) {
    auto surviving = stacks;
    apply_fgs_orbits(surviving, orbits, small_blind, big_blind, ante);
    return pko_icmbu_payouts(surviving, payouts, bounty_values);
}

LateRegOverlayResult late_reg_overlay_ev(int field_remaining, double prize_pool, double late_reg_fee,
                                         double starting_stack, double average_stack) {
    if (field_remaining < 1) {
        throw std::invalid_argument("lateRegOverlayEv: fieldRemaining must be >= 1");
    }
    require_finite_nonneg(prize_pool, "prizePool");
    require_positive(late_reg_fee, "lateRegFee");
    require_positive(starting_stack, "startingStack");
    require_positive(average_stack, "averageStack");

    LateRegOverlayResult r;
    r.overlay_ratio = (prize_pool / static_cast<double>(field_remaining)) / late_reg_fee;
    const double after_pool = prize_pool + late_reg_fee;

    std::vector<double> compressed;
    std::vector<double> prizes;
    if (field_remaining == 1) {
        compressed = {starting_stack, average_stack};
        prizes = {after_pool, 0.0};
    } else {
        compressed = {starting_stack, average_stack,
                      static_cast<double>(field_remaining - 1) * average_stack};
        prizes = {0.5 * after_pool, 0.3 * after_pool, 0.2 * after_pool};
    }
    r.icm_share = icm_expected_payouts(compressed, prizes)[0];
    r.register_ev = r.icm_share - late_reg_fee;
    return r;
}

SatelliteTicketEv winner_take_all_satellite_ev(const std::vector<double>& stacks, std::size_t hero,
                                               int ticket_count, double ticket_value) {
    if (stacks.empty() || hero >= stacks.size()) {
        throw std::invalid_argument("winnerTakeAllSatelliteEv: invalid hero index");
    }
    if (ticket_count < 1 || ticket_count > static_cast<int>(stacks.size())) {
        throw std::invalid_argument("winnerTakeAllSatelliteEv: ticketCount must be in 1..n");
    }
    require_finite_nonneg(ticket_value, "ticketValue");

    const auto now = icm_satellite_advance_probability(stacks, ticket_count);
    SatelliteTicketEv r;
    r.advance_prob = now[hero];
    r.ticket_ev = r.advance_prob * ticket_value;

    auto doubled = stacks;
    doubled[hero] *= 2.0;
    const double total_after = std::accumulate(doubled.begin(), doubled.end(), 0.0);
    if (total_after <= 0.0) {
        throw std::invalid_argument("winnerTakeAllSatelliteEv: positive chip total required");
    }
    const auto after = icm_satellite_advance_probability(doubled, ticket_count);
    r.chip_ev_if_double = (doubled[hero] / total_after) * static_cast<double>(ticket_count) * ticket_value;
    r.dollar_ev_if_double = after[hero] * ticket_value;
    return r;
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

IsoRaiseEv iso_raise_vs_limpers_ev(double pot, double iso_size, double limp_call, int n_limpers,
                                   double p_fold, const std::vector<double>& equities) {
    require_finite_nonneg(pot, "pot");
    require_finite_nonneg(iso_size, "isoSize");
    require_finite_nonneg(limp_call, "limpCall");
    if (n_limpers < 1 || n_limpers > 8) {
        throw std::invalid_argument("isoRaiseVsLimpersEv: nLimpers must be in 1..8");
    }
    require_unit(p_fold, "pFold");
    for (double e : equities) {
        require_unit(e, "equities");
    }

    const double p_call = 1.0 - p_fold;
    const int n = n_limpers;
    const int masks = 1 << n;
    double iso = 0.0;
    for (int mask = 0; mask < masks; ++mask) {
        int callers = 0;
        double p = 1.0;
        for (int i = 0; i < n; ++i) {
            if ((mask & (1 << i)) != 0) {
                ++callers;
                p *= p_call;
            } else {
                p *= p_fold;
            }
        }
        if (callers == 0) {
            iso += p * pot;
            continue;
        }
        const std::size_t eq_i = static_cast<std::size_t>(callers - 1);
        const double eq =
            eq_i < equities.size() ? equities[eq_i] : 1.0 / static_cast<double>(callers + 1);
        iso += p * showdown_chip_ev(eq, pot, iso_size, static_cast<double>(callers) * limp_call);
    }

    IsoRaiseEv r;
    r.iso_ev = iso;
    r.check_ev = pot / static_cast<double>(n + 1);
    r.fold_ev = 0.0;
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
