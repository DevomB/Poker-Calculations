#include "poker/fgs.hpp"

#include "poker/icm.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace poker {
namespace {

constexpr double kTinyIcm = 1e-12;

void validate_stacks_payouts(const std::vector<double>& stacks, const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    if (n == 0 || n > 31) {
        throw std::invalid_argument("FGS/ICM: need 1..31 players");
    }
    if (payouts.size() != n) {
        throw std::invalid_argument("FGS/ICM: payouts length must match stacks");
    }
    for (double s : stacks) {
        if (!std::isfinite(s) || s < 0.0) {
            throw std::invalid_argument("FGS/ICM: stacks must be finite and non-negative");
        }
    }
    for (double p : payouts) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("FGS/ICM: payouts must be finite and non-negative");
        }
    }
}

void validate_index_pair(std::size_t n, std::size_t hero, std::size_t villain) {
    if (hero >= n || villain >= n || hero == villain) {
        throw std::invalid_argument("FGS/ICM: invalid hero/villain indices");
    }
}

void validate_unit(const char* name, double x) {
    if (!std::isfinite(x) || x < 0.0 || x > 1.0) {
        throw std::invalid_argument(std::string(name) + " must be in [0,1]");
    }
}

void validate_nonneg(const char* name, double x) {
    if (!std::isfinite(x) || x < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and non-negative");
    }
}

[[nodiscard]] double orbit_cost(double small_blind, double big_blind, double ante) {
    return orbit_cost_chips(small_blind, big_blind, std::vector<double>{ante});
}

void deduct_average_orbit(std::vector<double>& stacks, double cost) {
    for (double& s : stacks) {
        if (s <= 0.0) {
            continue;
        }
        s = std::max(0.0, s - cost);
    }
}

/// Harville ICM on alive seats using the top-k prizes. Busted seats stay at $0.
[[nodiscard]] std::vector<double> icm_allowing_busts(const std::vector<double>& stacks,
                                                     const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    std::vector<double> ev(n, 0.0);
    std::vector<std::size_t> alive;
    alive.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] > 0.0) {
            alive.push_back(i);
        }
    }
    if (alive.empty()) {
        return ev;
    }
    std::vector<double> live_stacks;
    std::vector<double> live_payouts;
    live_stacks.reserve(alive.size());
    live_payouts.reserve(alive.size());
    for (std::size_t k = 0; k < alive.size(); ++k) {
        live_stacks.push_back(stacks[alive[k]]);
        live_payouts.push_back(payouts[k]);
    }
    const auto live_ev = icm_expected_payouts(live_stacks, live_payouts);
    for (std::size_t k = 0; k < alive.size(); ++k) {
        ev[alive[k]] = live_ev[k];
    }
    return ev;
}

void apply_orbit_count(std::vector<double>& stacks, int orbits, double cost) {
    if (orbits < 0) {
        throw std::invalid_argument("FGS: orbits must be >= 0");
    }
    for (int o = 0; o < orbits; ++o) {
        deduct_average_orbit(stacks, cost);
    }
}

void put_in_pot(std::vector<double>& s, std::size_t i, double amount) {
    if (amount > s[i] + 1e-15) {
        throw std::invalid_argument("FGS/ICM: post exceeds stack");
    }
    s[i] = std::max(0.0, s[i] - amount);
}

[[nodiscard]] std::pair<std::vector<double>, std::vector<double>> allin_terminals(
    const std::vector<double>& stacks, std::size_t hero, std::size_t villain, double pot,
    double hero_put, double villain_put) {
    validate_nonneg("pot", pot);
    validate_nonneg("heroPut", hero_put);
    validate_nonneg("villainPut", villain_put);
    auto win = stacks;
    auto lose = stacks;
    put_in_pot(win, hero, hero_put);
    put_in_pot(win, villain, villain_put);
    put_in_pot(lose, hero, hero_put);
    put_in_pot(lose, villain, villain_put);
    const double final_pot = pot + hero_put + villain_put;
    win[hero] += final_pot;
    lose[villain] += final_pot;
    return {std::move(win), std::move(lose)};
}

[[nodiscard]] double mix_hero_ev(const std::vector<double>& win, const std::vector<double>& lose,
                                 const std::vector<double>& payouts, std::size_t hero, double equity) {
    const auto ev_win = icm_allowing_busts(win, payouts);
    const auto ev_lose = icm_allowing_busts(lose, payouts);
    return equity * ev_win[hero] + (1.0 - equity) * ev_lose[hero];
}

}  // namespace

std::vector<double> future_game_simulation_payouts(const std::vector<double>& stacks,
                                                   const std::vector<double>& payouts, int orbits,
                                                   double small_blind, double big_blind, double ante) {
    validate_stacks_payouts(stacks, payouts);
    auto s = stacks;
    apply_orbit_count(s, orbits, orbit_cost(small_blind, big_blind, ante));
    return icm_allowing_busts(s, payouts);
}

FutureGrowthShareResult future_growth_share(const std::vector<double>& stacks, int orbits,
                                            double small_blind, double big_blind, double ante) {
    const std::size_t n = stacks.size();
    if (n == 0 || n > 31) {
        throw std::invalid_argument("FGS: need 1..31 players");
    }
    for (double s : stacks) {
        if (!std::isfinite(s) || s < 0.0) {
            throw std::invalid_argument("FGS: stacks must be finite and non-negative");
        }
    }
    if (orbits < 0) {
        throw std::invalid_argument("FGS: orbits must be >= 0");
    }
    const double cost = orbit_cost(small_blind, big_blind, ante);
    auto s = stacks;
    std::vector<double> paid(n, 0.0);
    std::vector<double> received(n, 0.0);
    for (int o = 0; o < orbits; ++o) {
        std::vector<std::size_t> paying;
        for (std::size_t i = 0; i < n; ++i) {
            if (s[i] > 0.0) {
                paying.push_back(i);
            }
        }
        if (paying.empty()) {
            break;
        }
        double collected = 0.0;
        for (std::size_t i : paying) {
            const double pay = std::min(s[i], cost);
            s[i] -= pay;
            paid[i] += pay;
            collected += pay;
        }
        std::vector<std::size_t> survivors;
        for (std::size_t i = 0; i < n; ++i) {
            if (s[i] > 0.0) {
                survivors.push_back(i);
            }
        }
        if (survivors.empty()) {
            continue;
        }
        const double share = collected / static_cast<double>(survivors.size());
        for (std::size_t i : survivors) {
            s[i] += share;
            received[i] += share;
        }
    }
    FutureGrowthShareResult out;
    out.net_growth.resize(n, 0.0);
    out.growth_share.resize(n, 0.0);
    double recv_survivors = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        out.net_growth[i] = received[i] - paid[i];
        if (s[i] > 0.0) {
            ++out.survivor_count;
            recv_survivors += received[i];
        }
    }
    if (recv_survivors > 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            if (s[i] > 0.0) {
                out.growth_share[i] = received[i] / recv_survivors;
            }
        }
    }
    return out;
}

std::vector<double> icm_payouts_after_blind_post(const std::vector<double>& stacks,
                                                 const std::vector<double>& payouts, std::size_t hero,
                                                 double hero_post, const std::vector<double>& posts) {
    validate_stacks_payouts(stacks, payouts);
    const std::size_t n = stacks.size();
    if (hero >= n) {
        throw std::invalid_argument("icmPayoutsAfterBlindPost: invalid hero index");
    }
    if (posts.size() != n) {
        throw std::invalid_argument("icmPayoutsAfterBlindPost: posts length must match stacks");
    }
    validate_nonneg("heroPost", hero_post);
    auto s = stacks;
    for (std::size_t i = 0; i < n; ++i) {
        const double post = (i == hero) ? hero_post : posts[i];
        validate_nonneg("post", post);
        s[i] = std::max(0.0, s[i] - post);
    }
    return icm_allowing_busts(s, payouts);
}

IcmDecisionEv icm_jam_vs_fold_ev(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                 std::size_t hero, std::size_t villain, double pot, double fold_equity,
                                 double equity_when_called) {
    validate_stacks_payouts(stacks, payouts);
    validate_index_pair(stacks.size(), hero, villain);
    validate_nonneg("pot", pot);
    validate_unit("foldEquity", fold_equity);
    validate_unit("equityWhenCalled", equity_when_called);
    const auto now = icm_allowing_busts(stacks, payouts);
    IcmDecisionEv out;
    out.fold_ev = now[hero];
    auto folded = stacks;
    folded[hero] += pot;
    const double fold_collect = icm_allowing_busts(folded, payouts)[hero];
    const double put = std::min(stacks[hero], stacks[villain]);
    const auto [win, lose] = allin_terminals(stacks, hero, villain, pot, put, put);
    const double called = mix_hero_ev(win, lose, payouts, hero, equity_when_called);
    out.take_ev = fold_equity * fold_collect + (1.0 - fold_equity) * called;
    out.delta = out.take_ev - out.fold_ev;
    return out;
}

IcmDecisionEv icm_call_vs_fold_ev(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                  std::size_t hero, std::size_t villain, double pot, double call_amount,
                                  double hero_equity) {
    validate_stacks_payouts(stacks, payouts);
    validate_index_pair(stacks.size(), hero, villain);
    validate_nonneg("pot", pot);
    validate_nonneg("callAmount", call_amount);
    validate_unit("heroEquity", hero_equity);
    const auto now = icm_allowing_busts(stacks, payouts);
    IcmDecisionEv out;
    out.fold_ev = now[hero];
    const double hero_put = std::min(call_amount, stacks[hero]);
    const double villain_put = std::min(stacks[villain], hero_put);
    const auto [win, lose] = allin_terminals(stacks, hero, villain, pot, hero_put, villain_put);
    out.take_ev = mix_hero_ev(win, lose, payouts, hero, hero_equity);
    out.delta = out.take_ev - out.fold_ev;
    return out;
}

double icm_calling_bubble_factor(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                 std::size_t hero, std::size_t villain, double chips_at_risk) {
    validate_stacks_payouts(stacks, payouts);
    validate_index_pair(stacks.size(), hero, villain);
    validate_nonneg("chipsAtRisk", chips_at_risk);
    const double risk = std::min({chips_at_risk, stacks[hero], stacks[villain]});
    const auto now = icm_allowing_busts(stacks, payouts);
    auto win = stacks;
    auto lose = stacks;
    win[hero] += risk;
    win[villain] = std::max(0.0, win[villain] - risk);
    lose[hero] = std::max(0.0, lose[hero] - risk);
    lose[villain] += risk;
    const double ev_now = now[hero];
    const double ev_win = icm_allowing_busts(win, payouts)[hero];
    const double ev_lose = icm_allowing_busts(lose, payouts)[hero];
    const double loss = ev_now - ev_lose;
    const double gain = ev_win - ev_now;
    if (std::abs(gain) < kTinyIcm) {
        if (std::abs(loss) < kTinyIcm) {
            return 1.0;
        }
        return std::copysign(std::numeric_limits<double>::infinity(), loss);
    }
    return loss / gain;
}

std::vector<double> fgs_payouts_blind_schedule(const std::vector<double>& stacks,
                                               const std::vector<double>& payouts,
                                               const std::vector<double>& small_blinds,
                                               const std::vector<double>& big_blinds,
                                               const std::vector<double>& antes,
                                               const std::vector<double>& orbits_at_level) {
    validate_stacks_payouts(stacks, payouts);
    const std::size_t levels = small_blinds.size();
    if (big_blinds.size() != levels || antes.size() != levels || orbits_at_level.size() != levels) {
        throw std::invalid_argument("fgsPayoutsBlindSchedule: schedule arrays must be the same length");
    }
    auto s = stacks;
    for (std::size_t i = 0; i < levels; ++i) {
        if (!std::isfinite(orbits_at_level[i]) || orbits_at_level[i] < 0.0) {
            throw std::invalid_argument("fgsPayoutsBlindSchedule: orbitsAtLevel must be >= 0");
        }
        const int orbits = static_cast<int>(orbits_at_level[i]);
        apply_orbit_count(s, orbits, orbit_cost(small_blinds[i], big_blinds[i], antes[i]));
    }
    return icm_allowing_busts(s, payouts);
}

IcmStallingEvResult icm_stalling_ev(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                    std::size_t hero, double small_blind, double big_blind, double ante,
                                    bool model_collision) {
    validate_stacks_payouts(stacks, payouts);
    if (hero >= stacks.size()) {
        throw std::invalid_argument("icmStallingEv: invalid hero index");
    }
    IcmStallingEvResult out;
    out.now_ev = icm_allowing_busts(stacks, payouts)[hero];
    out.stall_ev =
        future_game_simulation_payouts(stacks, payouts, 1, small_blind, big_blind, ante)[hero];
    out.stalling_premium = out.stall_ev - out.now_ev;
    if (!model_collision) {
        return out;
    }
    std::vector<std::size_t> alive;
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i] > 0.0) {
            alive.push_back(i);
        }
    }
    if (alive.size() < 2) {
        out.collision_ev = out.now_ev;
        out.collision_modeled = false;
        return out;
    }
    std::sort(alive.begin(), alive.end(), [&](std::size_t a, std::size_t b) {
        if (stacks[a] != stacks[b]) {
            return stacks[a] < stacks[b];
        }
        return a < b;
    });
    const std::size_t a = alive[0];
    const std::size_t b = alive[1];
    const double risk = std::min(stacks[a], stacks[b]);
    auto a_wins = stacks;
    auto b_wins = stacks;
    a_wins[a] += risk;
    a_wins[b] = std::max(0.0, a_wins[b] - risk);
    b_wins[b] += risk;
    b_wins[a] = std::max(0.0, b_wins[a] - risk);
    out.collision_ev = 0.5 * icm_allowing_busts(a_wins, payouts)[hero] +
                       0.5 * icm_allowing_busts(b_wins, payouts)[hero];
    out.collision_modeled = true;
    return out;
}

IcmPayJumpSurvivalResult icm_pay_jump_survival_ev(const std::vector<double>& stacks,
                                                  const std::vector<double>& payouts, std::size_t hero,
                                                  const std::string& bust_chips) {
    validate_stacks_payouts(stacks, payouts);
    if (hero >= stacks.size()) {
        throw std::invalid_argument("icmPayJumpSurvivalEv: invalid hero index");
    }
    if (bust_chips != "vanish" && bust_chips != "chipLeader") {
        throw std::invalid_argument("icmPayJumpSurvivalEv: bustChips must be vanish or chipLeader");
    }
    IcmPayJumpSurvivalResult out;
    out.now_ev = icm_allowing_busts(stacks, payouts)[hero];
    std::size_t shortest = stacks.size();
    double shortest_stack = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        if (i == hero || stacks[i] <= 0.0) {
            continue;
        }
        if (stacks[i] < shortest_stack) {
            shortest_stack = stacks[i];
            shortest = i;
        }
    }
    if (shortest >= stacks.size()) {
        out.after_bust_ev = out.now_ev;
        out.ladder_delta = 0.0;
        out.busted_index = hero;
        return out;
    }
    out.busted_index = shortest;
    auto after = stacks;
    const double chips = after[shortest];
    after[shortest] = 0.0;
    if (bust_chips == "chipLeader") {
        std::size_t leader = 0;
        for (std::size_t i = 1; i < after.size(); ++i) {
            if (after[i] > after[leader]) {
                leader = i;
            }
        }
        after[leader] += chips;
    }
    out.after_bust_ev = icm_allowing_busts(after, payouts)[hero];
    out.ladder_delta = out.after_bust_ev - out.now_ev;
    return out;
}

IcmDeadPotDollarEvResult icm_dead_pot_dollar_ev(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts, std::size_t hero,
                                                double dead_chips) {
    validate_stacks_payouts(stacks, payouts);
    if (hero >= stacks.size()) {
        throw std::invalid_argument("icmDeadPotDollarEv: invalid hero index");
    }
    validate_nonneg("deadChips", dead_chips);
    IcmDeadPotDollarEvResult out;
    out.now_ev = icm_allowing_busts(stacks, payouts)[hero];
    auto win = stacks;
    win[hero] += dead_chips;
    out.win_ev = icm_allowing_busts(win, payouts)[hero];
    out.delta = out.win_ev - out.now_ev;
    return out;
}

}  // namespace poker
