#include "poker/fgs.hpp"

#include "poker/icm.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

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

void apply_orbit_count(std::vector<double>& stacks, int orbits, double cost) {
    if (orbits < 0) {
        throw std::invalid_argument("FGS: orbits must be >= 0");
    }
    for (int o = 0; o < orbits; ++o) {
        deduct_average_orbit(stacks, cost);
    }
}

}  // namespace

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
    return icm_expected_payouts_allowing_busts(s, payouts);
}

double icm_calling_bubble_factor(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                 std::size_t hero, std::size_t villain, double chips_at_risk) {
    validate_stacks_payouts(stacks, payouts);
    validate_index_pair(stacks.size(), hero, villain);
    validate_nonneg("chipsAtRisk", chips_at_risk);
    const double risk = std::min({chips_at_risk, stacks[hero], stacks[villain]});
    const auto now = icm_expected_payouts_allowing_busts(stacks, payouts);
    auto win = stacks;
    auto lose = stacks;
    win[hero] += risk;
    win[villain] = std::max(0.0, win[villain] - risk);
    lose[hero] = std::max(0.0, lose[hero] - risk);
    lose[villain] += risk;
    const double ev_now = now[hero];
    const double ev_win = icm_expected_payouts_allowing_busts(win, payouts)[hero];
    const double ev_lose = icm_expected_payouts_allowing_busts(lose, payouts)[hero];
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
    return icm_expected_payouts_allowing_busts(s, payouts);
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
    out.now_ev = icm_expected_payouts_allowing_busts(stacks, payouts)[hero];
    auto win = stacks;
    win[hero] += dead_chips;
    out.win_ev = icm_expected_payouts_allowing_busts(win, payouts)[hero];
    out.delta = out.win_ev - out.now_ev;
    return out;
}

}  // namespace poker
