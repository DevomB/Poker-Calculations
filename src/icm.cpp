#include "poker/bit_utils.hpp"
#include "poker/icm.hpp"
#include "poker/range.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace poker {

namespace {

void require_player_count(std::size_t n, const char* what) {
    if (n == 0 || n > 31) {
        throw std::invalid_argument(std::string(what) + ": need 1..31 players");
    }
}

void require_positive_stacks(const std::vector<double>& stacks, const char* what) {
    for (double s : stacks) {
        if (!std::isfinite(s) || !(s > 0.0)) {
            throw std::invalid_argument(std::string(what) + ": stacks must be finite and positive");
        }
    }
}

void require_payouts(const std::vector<double>& payouts, std::size_t n, const char* what) {
    if (payouts.size() != n) {
        throw std::invalid_argument(std::string(what) + ": payouts vector must match number of players");
    }
    for (double p : payouts) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument(std::string(what) + ": payouts must be finite and non-negative");
        }
    }
}

/// Places that need resolving: those paying something other than last place. Every player is
/// paid at least the last-place prize, so EV_i = last + sum_{j < K} P(i at j) (payout_j - last).
std::size_t places_paying_above_last(const std::vector<double>& payouts) {
    std::size_t k = 0;
    const double last = payouts.back();
    for (std::size_t j = 0; j + 1 < payouts.size(); ++j) {
        if (payouts[j] != last) {
            k = j + 1;
        }
    }
    return k;
}

std::vector<double> payouts_from_placements(const std::vector<std::vector<double>>& placement,
                                            const std::vector<double>& payouts) {
    const double last = payouts.back();
    std::vector<double> ev(placement.size(), last);
    for (std::size_t i = 0; i < placement.size(); ++i) {
        for (std::size_t j = 0; j < placement[i].size(); ++j) {
            ev[i] += placement[i][j] * (payouts[j] - last);
        }
    }
    return ev;
}

}  // namespace

std::vector<std::vector<double>> harville_top_places(const std::vector<double>& weights, std::size_t places) {
    const std::size_t n = weights.size();
    require_player_count(n, "Harville placement");
    if (places > n) {
        throw std::invalid_argument("Harville placement: more places than players");
    }
    for (double w : weights) {
        if (!std::isfinite(w) || w < 0.0) {
            throw std::invalid_argument("Harville placement: weights must be finite and non-negative");
        }
    }
    std::vector<std::vector<double>> placement(n, std::vector<double>(places, 0.0));
    if (places == 0) {
        return placement;
    }
    // A state is the set of players already placed; `j` = its size is the next place to fill. The
    // number of states up to place `places` is sum_{j < places} C(n, j); bound it before any work.
    double states = 0.0;
    double binom = 1.0;
    for (std::size_t j = 0; j < places; ++j) {
        states += binom;
        binom = binom * static_cast<double>(n - j) / static_cast<double>(j + 1);
    }
    if (n > kHarvilleDenseMaxPlayers && states > static_cast<double>(kHarvilleMaxStates)) {
        throw std::invalid_argument("Harville placement: resolving " + std::to_string(places) + " places for " +
                                    std::to_string(n) + " players needs more than " +
                                    std::to_string(kHarvilleMaxStates) +
                                    " subset states (supported: every place for up to " +
                                    std::to_string(kHarvilleDenseMaxPlayers) +
                                    " players, or fewer places for larger fields)");
    }
    const std::uint32_t full = (1U << n) - 1U;  // n <= 31
    // From `placed` with probability `prob`: each remaining player takes place j with probability
    // weight / (sum of remaining weights), scaled by the largest remaining weight so huge or tiny
    // weights never overflow or vanish against players already gone. If every remaining weight is
    // 0, the remaining players are equally likely (a tie with no information).
    const auto expand = [&](std::uint32_t placed, std::size_t j, double prob, auto&& emit) {
        const std::uint32_t remaining = full & ~placed;
        double largest = 0.0;
        int count = 0;
        for (std::uint32_t m = remaining; m != 0; m &= m - 1) {
            largest = std::max(largest, weights[static_cast<std::size_t>(std::countr_zero(m))]);
            ++count;
        }
        const int scale = weight_scale_exponent(largest);
        double sum = 0.0;
        for (std::uint32_t m = remaining; m != 0; m &= m - 1) {
            sum += std::ldexp(weights[static_cast<std::size_t>(std::countr_zero(m))], scale);
        }
        for (std::uint32_t m = remaining; m != 0; m &= m - 1) {
            const int i = std::countr_zero(m);
            const double share = sum > 0.0 ? std::ldexp(weights[static_cast<std::size_t>(i)], scale) / sum
                                           : 1.0 / static_cast<double>(count);
            const double t = prob * share;
            if (t == 0.0) {
                continue;
            }
            placement[static_cast<std::size_t>(i)][j] += t;
            if (j + 1 < places) {
                emit(placed | (1U << i), t);
            }
        }
    };
    if (n <= kHarvilleDenseMaxPlayers) {
        // Dense table over all subsets; a superset always has a larger index, so one increasing
        // pass sees every state after all of its predecessors.
        std::vector<double> f(std::size_t{1} << n, 0.0);
        f[0] = 1.0;
        for (std::uint32_t placed = 0; placed <= full; ++placed) {
            const double prob = f[placed];
            const std::size_t j = static_cast<std::size_t>(popcount_u32(placed));
            if (prob != 0.0 && j < places) {
                expand(placed, j, prob, [&](std::uint32_t next, double t) { f[next] += t; });
            }
            if (placed == full) {
                break;
            }
        }
        return placement;
    }
    // Larger fields: only the subsets of size < places, one layer at a time in mask order (so the
    // summation order, and the result, do not depend on hash-table iteration order).
    std::vector<std::pair<std::uint32_t, double>> layer{{0U, 1.0}};
    for (std::size_t j = 0; j < places; ++j) {
        std::unordered_map<std::uint32_t, double> next;
        for (const auto& [placed, prob] : layer) {
            expand(placed, j, prob, [&](std::uint32_t nxt, double t) { next[nxt] += t; });
        }
        layer.assign(next.begin(), next.end());
        std::sort(layer.begin(), layer.end());
    }
    return placement;
}

std::vector<std::vector<double>> icm_harville_placement_probabilities(const std::vector<double>& stacks) {
    require_player_count(stacks.size(), "ICM");
    require_positive_stacks(stacks, "ICM");
    return harville_top_places(stacks, stacks.size());
}

std::vector<double> icm_win_probabilities_harville(const std::vector<double>& stacks) {
    // First place is just each stack's share; no enumeration.
    require_player_count(stacks.size(), "ICM");
    require_positive_stacks(stacks, "ICM");
    const auto first = harville_top_places(stacks, 1);
    std::vector<double> win(stacks.size(), 0.0);
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        win[i] = first[i][0];
    }
    return win;
}

std::vector<double> icm_top_k_finish_probabilities(const std::vector<double>& stacks, int k) {
    const std::size_t n = stacks.size();
    require_player_count(n, "ICM");
    require_positive_stacks(stacks, "ICM");
    if (k < 1 || static_cast<std::size_t>(k) > n) {
        throw std::invalid_argument("ICM top-k: k must be between 1 and number of players inclusive");
    }
    if (static_cast<std::size_t>(k) == n) {
        return std::vector<double>(n, 1.0);
    }
    const auto placement = harville_top_places(stacks, static_cast<std::size_t>(k));
    std::vector<double> out(n, 0.0);
    std::transform(placement.begin(), placement.end(), out.begin(),
                   [](const auto& row) { return std::accumulate(row.begin(), row.end(), 0.0); });
    return out;
}

std::vector<double> icm_last_place_probabilities_harville(const std::vector<double>& stacks) {
    const auto placement = icm_harville_placement_probabilities(stacks);
    const std::size_t n = stacks.size();
    std::vector<double> out(n, 0.0);
    const std::size_t last = n - 1;
    std::transform(placement.begin(), placement.end(), out.begin(),
                   [last](const auto& row) { return row[last]; });
    return out;
}

std::vector<double> icm_expected_payouts(const std::vector<double>& stacks,
                                         const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    require_player_count(n, "ICM");
    require_payouts(payouts, n, "ICM");
    require_positive_stacks(stacks, "ICM");
    return payouts_from_placements(harville_top_places(stacks, places_paying_above_last(payouts)), payouts);
}

/// Harville ICM on alive seats using the top-k prizes. Busted seats have already finished and
/// split the bottom prizes, so the prize pool is conserved.
std::vector<double> icm_expected_payouts_allowing_busts(const std::vector<double>& stacks,
                                                       const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    if (payouts.size() != n) {
        throw std::invalid_argument("ICM: payouts vector must match number of players");
    }
    for (double p : payouts) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("ICM: payouts must be finite and non-negative");
        }
    }
    std::vector<double> ev(n, 0.0);
    std::vector<std::size_t> alive;
    std::vector<std::size_t> dead;
    alive.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] > 0.0) {
            alive.push_back(i);
        } else {
            dead.push_back(i);
        }
    }
    if (!dead.empty()) {
        double dead_prize = 0.0;
        for (std::size_t r = n - dead.size(); r < n; ++r) {
            dead_prize += payouts[r];
        }
        const double each = dead_prize / static_cast<double>(dead.size());
        for (std::size_t i : dead) {
            ev[i] = each;
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

double icm_pairwise_bubble_factor(const std::vector<double>& stacks,
                                  const std::vector<double>& payouts, std::size_t hero,
                                  std::size_t villain, double pot_chips) {
    if (!std::isfinite(pot_chips) || pot_chips < 0.0) {
        throw std::invalid_argument("potChips must be finite and non-negative");
    }
    const std::size_t n = stacks.size();
    if (hero >= n || villain >= n || hero == villain) {
        throw std::invalid_argument("ICM bubble factor: invalid hero/villain indices");
    }
    const auto base = icm_expected_payouts(stacks, payouts);
    auto lose = stacks;
    lose[hero] -= pot_chips;
    lose[villain] += pot_chips;
    auto win = stacks;
    win[hero] += pot_chips;
    win[villain] -= pot_chips;
    for (double s : lose) {
        if (s < 0.0) {
            throw std::invalid_argument("ICM bubble factor: stack would go negative after loss");
        }
    }
    for (double s : win) {
        if (s < 0.0) {
            throw std::invalid_argument("ICM bubble factor: stack would go negative after win");
        }
    }
    // Either side may bust; a busted seat takes the bottom prize it can no longer avoid.
    const auto ev_lose = icm_expected_payouts_allowing_busts(lose, payouts);
    const auto ev_win = icm_expected_payouts_allowing_busts(win, payouts);
    const double loss = base[hero] - ev_lose[hero];
    const double gain = ev_win[hero] - base[hero];
    if (std::abs(gain) < 1e-12) {
        throw std::invalid_argument("ICM bubble factor: marginal gain is zero (degenerate)");
    }
    return loss / gain;
}

std::vector<double> icm_expected_payouts_weitzman(const std::vector<double>& stacks,
                                                 const std::vector<double>& payouts, double alpha) {
    const std::size_t n = stacks.size();
    if (n == 0) {
        throw std::invalid_argument("ICM Weitzman: need at least one player");
    }
    if (payouts.size() != n) {
        throw std::invalid_argument("ICM Weitzman: payouts vector must match number of players");
    }
    if (!std::isfinite(alpha) || alpha <= 0.0) {
        throw std::invalid_argument("ICM Weitzman: alpha must be finite and positive");
    }
    std::vector<double> util(n, 0.0);
    double sum_util = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double s = stacks[i];
        if (!std::isfinite(s) || s < 0.0) {
            throw std::invalid_argument("ICM Weitzman: stacks must be finite and non-negative");
        }
        if (s <= 0.0) {
            util[i] = 0.0;
            continue;
        }
        util[i] = std::log(s);
    }
    // stack^alpha / sum stack^alpha, formed as exp(alpha (ln s - ln s_max)) so neither the powers
    // nor their sum overflow or underflow to 0 when stacks are huge or tiny.
    double log_max = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] > 0.0) {
            log_max = std::max(log_max, util[i]);
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        util[i] = stacks[i] > 0.0 ? std::exp(alpha * (util[i] - log_max)) : 0.0;
        sum_util += util[i];
    }
    if (sum_util <= 0.0) {
        throw std::invalid_argument("ICM Weitzman: positive total utility required");
    }
    std::vector<double> ev(n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        const double prize = payouts[k];
        if (!std::isfinite(prize) || prize < 0.0) {
            throw std::invalid_argument("ICM Weitzman: payouts must be finite and non-negative");
        }
        for (std::size_t i = 0; i < n; ++i) {
            ev[i] += prize * (util[i] / sum_util);
        }
    }
    return ev;
}

std::vector<double> icm_harville_skill_adjusted_payouts(const std::vector<double>& stacks,
                                                        const std::vector<double>& payouts,
                                                        const std::vector<double>& skill_weights,
                                                        double blend) {
    const std::size_t n = stacks.size();
    require_player_count(n, "ICM skill-adjusted");
    if (skill_weights.size() != n) {
        throw std::invalid_argument("ICM skill-adjusted: vector lengths must match");
    }
    require_payouts(payouts, n, "ICM skill-adjusted");
    require_positive_stacks(stacks, "ICM skill-adjusted");
    if (!std::isfinite(blend) || blend < 0.0 || blend > 1.0) {
        throw std::invalid_argument("ICM skill-adjusted: blend must be in [0,1]");
    }
    for (double w : skill_weights) {
        if (!std::isfinite(w) || w < 0.0) {
            throw std::invalid_argument("ICM skill-adjusted: skill weights must be finite and non-negative");
        }
    }
    // First-place weights: blend of stack shares and skill shares, each normalized with the scale of
    // its own largest entry so huge or tiny inputs keep their proportions.
    const auto shares = [](const std::vector<double>& v) {
        double largest = 0.0;
        for (double x : v) {
            largest = std::max(largest, x);
        }
        const int k = weight_scale_exponent(largest);
        double sum = 0.0;
        for (double x : v) {
            sum += std::ldexp(x, k);
        }
        std::vector<double> out(v.size(), 0.0);
        if (sum > 0.0) {
            for (std::size_t i = 0; i < v.size(); ++i) {
                out[i] = std::ldexp(v[i], k) / sum;
            }
        }
        return out;
    };
    const std::vector<double> stack_share = shares(stacks);
    const std::vector<double> skill_share = shares(skill_weights);
    if (blend > 0.0 && std::all_of(skill_share.begin(), skill_share.end(), [](double x) { return x == 0.0; })) {
        throw std::invalid_argument("ICM skill-adjusted: skill weights need a positive sum when blend > 0");
    }
    std::vector<double> w(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        w[i] = (1.0 - blend) * stack_share[i] + blend * skill_share[i];
    }
    // With blend = 1 a zero skill weight is a zero first-place weight: such seats finish after every
    // positive-weight seat, in a uniformly random order among themselves.
    return payouts_from_placements(harville_top_places(w, places_paying_above_last(payouts)), payouts);
}

std::vector<double> icm_equal_chop_payouts(const std::vector<double>& payouts) {
    if (payouts.empty()) {
        throw std::invalid_argument("payouts must be non-empty");
    }
    const double total = std::accumulate(payouts.begin(), payouts.end(), 0.0, [](double acc, double p) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("payouts must be finite and non-negative");
        }
        return acc + p;
    });
    const double each = total / static_cast<double>(payouts.size());
    return std::vector<double>(payouts.size(), each);
}

std::vector<double> icm_chop_surplus_vs_equal_split(const std::vector<double>& stacks,
                                                  const std::vector<double>& payouts) {
    const std::vector<double> icm = icm_expected_payouts(stacks, payouts);
    const std::vector<double> eq = icm_equal_chop_payouts(payouts);
    std::vector<double> out(icm.size());
    std::transform(icm.begin(), icm.end(), eq.begin(), out.begin(), std::minus<>());
    return out;
}

double icm_total_prize_pool(const std::vector<double>& payouts) {
    return std::accumulate(payouts.begin(), payouts.end(), 0.0, [](double acc, double p) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("payouts must be finite and non-negative");
        }
        return acc + p;
    });
}

std::vector<double> icm_deal_ev_per_chip(const std::vector<double>& stacks,
                                         const std::vector<double>& payouts) {
    const std::vector<double> ev = icm_expected_payouts(stacks, payouts);
    std::vector<double> out(ev.size());
    for (std::size_t i = 0; i < ev.size(); ++i) {
        if (stacks[i] <= 0.0) {
            throw std::invalid_argument("stacks must be positive");
        }
        out[i] = ev[i] / stacks[i];
    }
    return out;
}

std::vector<double> icm_satellite_advance_probability(const std::vector<double>& stacks,
                                                      int paid_places) {
    return icm_top_k_finish_probabilities(stacks, paid_places);
}

double icm_payout_structure_gini(const std::vector<double>& payouts) {
    if (payouts.empty()) {
        throw std::invalid_argument("payouts must be non-empty");
    }
    std::vector<double> sorted = payouts;
    for (double p : sorted) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("payouts must be finite and non-negative");
        }
    }
    std::sort(sorted.begin(), sorted.end());
    const double n = static_cast<double>(sorted.size());
    const double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    if (sum <= 0.0) {
        return 0.0;
    }
    double num = 0.0;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        num += (2.0 * static_cast<double>(i + 1) - n - 1.0) * sorted[i];
    }
    return num / (n * sum);
}

double icm_chip_leader_premium_vs_equal_chop(const std::vector<double>& stacks,
                                             const std::vector<double>& payouts) {
    if (stacks.empty()) {
        throw std::invalid_argument("stacks must be non-empty");
    }
    const auto leader_it = std::max_element(stacks.begin(), stacks.end());
    const std::size_t leader = static_cast<std::size_t>(std::distance(stacks.begin(), leader_it));
    const std::vector<double> surplus = icm_chop_surplus_vs_equal_split(stacks, payouts);
    const std::vector<double> eq = icm_equal_chop_payouts(payouts);
    if (eq[leader] <= 0.0) {
        return 0.0;
    }
    return surplus[leader] / eq[leader];
}

std::vector<double> icm_expected_payouts_delta_from_chip_chop(const std::vector<double>& stacks,
                                                              const std::vector<double>& payouts) {
    const std::vector<double> icm = icm_expected_payouts(stacks, payouts);
    const double total = icm_total_prize_pool(payouts);
    double sum_s = 0.0;
    for (double s : stacks) {
        if (s < 0.0 || !std::isfinite(s)) {
            throw std::invalid_argument("stacks must be finite and non-negative");
        }
        sum_s += s;
    }
    if (sum_s <= 0.0) {
        throw std::invalid_argument("sum of stacks must be positive");
    }
    std::vector<double> out(icm.size());
    for (std::size_t i = 0; i < icm.size(); ++i) {
        const double chip_chop = total * (stacks[i] / sum_s);
        out[i] = icm[i] - chip_chop;
    }
    return out;
}

}  // namespace poker
