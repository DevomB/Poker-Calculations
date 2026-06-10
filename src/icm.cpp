#include "poker/bit_utils.hpp"
#include "poker/icm.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace poker {

namespace {

[[nodiscard]] int lowest_bit_index(std::uint32_t mask) {
    for (int i = 0; i < 32; ++i) {
        if ((mask & (1U << i)) != 0) {
            return i;
        }
    }
    return 0;
}

[[nodiscard]] double sum_stacks_mask(const std::vector<double>& stacks, std::uint32_t mask) {
    double s = 0.0;
    for (std::uint32_t m = mask; m != 0; ) {
        const int i = lowest_bit_index(m);
        s += stacks[static_cast<std::size_t>(i)];
        m &= m - 1;
    }
    return s;
}

void harville_placement_recur(const std::vector<double>& stacks, std::uint32_t mask, int next_rank,
                              double path_prob, std::vector<std::vector<double>>& placement) {
    if (path_prob < 1e-300) {
        return;
    }
    const int alive = popcount_u32(mask);
    if (alive == 1) {
        const int i = lowest_bit_index(mask);
        placement[static_cast<std::size_t>(i)][static_cast<std::size_t>(next_rank - 1)] += path_prob;
        return;
    }
    const double sum_chips = sum_stacks_mask(stacks, mask);
    if (sum_chips <= 0.0) {
        throw std::invalid_argument("ICM: positive total stack required among remaining players");
    }
    for (std::uint32_t m = mask; m != 0; ) {
        const int i = lowest_bit_index(m);
        const double take = stacks[static_cast<std::size_t>(i)] / sum_chips * path_prob;
        placement[static_cast<std::size_t>(i)][static_cast<std::size_t>(next_rank - 1)] += take;
        const std::uint32_t sub = mask & ~(1U << i);
        harville_placement_recur(stacks, sub, next_rank + 1, take, placement);
        m &= m - 1;
    }
}

void harville_placement_recur_weighted(const std::vector<double>& first_place_weights,
                                       std::uint32_t mask, int next_rank, double path_prob,
                                       std::vector<std::vector<double>>& placement) {
    if (path_prob < 1e-300) {
        return;
    }
    const int alive = popcount_u32(mask);
    if (alive == 1) {
        const int i = lowest_bit_index(mask);
        placement[static_cast<std::size_t>(i)][static_cast<std::size_t>(next_rank - 1)] += path_prob;
        return;
    }
    double sum_w = 0.0;
    for (std::uint32_t m = mask; m != 0; ) {
        const int i = lowest_bit_index(m);
        sum_w += first_place_weights[static_cast<std::size_t>(i)];
        m &= m - 1;
    }
    if (sum_w <= 0.0) {
        throw std::invalid_argument("ICM skill-adjusted: positive weight sum required");
    }
    for (std::uint32_t m = mask; m != 0; ) {
        const int i = lowest_bit_index(m);
        const double take =
            first_place_weights[static_cast<std::size_t>(i)] / sum_w * path_prob;
        placement[static_cast<std::size_t>(i)][static_cast<std::size_t>(next_rank - 1)] += take;
        const std::uint32_t sub = mask & ~(1U << i);
        harville_placement_recur_weighted(first_place_weights, sub, next_rank + 1, take, placement);
        m &= m - 1;
    }
}

}  // namespace

std::vector<std::vector<double>> icm_harville_placement_probabilities(const std::vector<double>& stacks) {
    const std::size_t n = stacks.size();
    if (n == 0 || n > 31) {
        throw std::invalid_argument("ICM: need 1..31 players");
    }
    for (double s : stacks) {
        if (!std::isfinite(s) || s < 0.0) {
            throw std::invalid_argument("ICM: stacks must be finite and non-negative");
        }
    }
    std::uint32_t mask = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] <= 0.0) {
            throw std::invalid_argument("ICM: all stacks must be positive for Harville weighting");
        }
        mask |= (1U << static_cast<unsigned>(i));
    }
    std::vector<std::vector<double>> placement(n, std::vector<double>(n, 0.0));
    harville_placement_recur(stacks, mask, 1, 1.0, placement);
    return placement;
}

std::vector<double> icm_win_probabilities_harville(const std::vector<double>& stacks) {
    const auto p = icm_harville_placement_probabilities(stacks);
    const std::size_t n = stacks.size();
    std::vector<double> win(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        win[i] = p[i][0];
    }
    return win;
}

std::vector<double> icm_top_k_finish_probabilities(const std::vector<double>& stacks, int k) {
    const std::size_t n = stacks.size();
    if (k < 1 || static_cast<std::size_t>(k) > n) {
        throw std::invalid_argument("ICM top-k: k must be between 1 and number of players inclusive");
    }
    const auto placement = icm_harville_placement_probabilities(stacks);
    std::vector<double> out(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (int r = 0; r < k; ++r) {
            out[i] += placement[i][static_cast<std::size_t>(r)];
        }
    }
    return out;
}

std::vector<double> icm_last_place_probabilities_harville(const std::vector<double>& stacks) {
    const auto placement = icm_harville_placement_probabilities(stacks);
    const std::size_t n = stacks.size();
    std::vector<double> out(n, 0.0);
    const std::size_t last = n - 1;
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = placement[i][last];
    }
    return out;
}

std::vector<double> icm_expected_payouts(const std::vector<double>& stacks,
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
    const auto placement = icm_harville_placement_probabilities(stacks);
    std::vector<double> ev(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t r = 0; r < n; ++r) {
            ev[i] += placement[i][r] * payouts[r];
        }
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
    const auto ev_lose = icm_expected_payouts(lose, payouts);
    const auto ev_win = icm_expected_payouts(win, payouts);
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
        util[i] = std::pow(s, alpha);
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
    if (payouts.size() != n || skill_weights.size() != n) {
        throw std::invalid_argument("ICM skill-adjusted: vector lengths must match");
    }
    if (!std::isfinite(blend) || blend < 0.0 || blend > 1.0) {
        throw std::invalid_argument("ICM skill-adjusted: blend must be in [0,1]");
    }
    double sum_s = 0.0;
    double sum_pi = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] <= 0.0) {
            throw std::invalid_argument("ICM skill-adjusted: positive stacks required");
        }
        if (skill_weights[i] < 0.0 || !std::isfinite(skill_weights[i])) {
            throw std::invalid_argument("ICM skill-adjusted: skill weights must be non-negative");
        }
        sum_s += stacks[i];
        sum_pi += skill_weights[i];
    }
    if (sum_s <= 0.0 || sum_pi <= 0.0) {
        throw std::invalid_argument("ICM skill-adjusted: positive sums required");
    }
    std::vector<double> w(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        w[i] = (1.0 - blend) * (stacks[i] / sum_s) + blend * (skill_weights[i] / sum_pi);
    }
    std::uint32_t mask = 0;
    for (std::size_t i = 0; i < n; ++i) {
        mask |= (1U << static_cast<unsigned>(i));
    }
    std::vector<std::vector<double>> placement(n, std::vector<double>(n, 0.0));
    harville_placement_recur_weighted(w, mask, 1, 1.0, placement);
    std::vector<double> ev(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t r = 0; r < n; ++r) {
            ev[i] += placement[i][r] * payouts[r];
        }
    }
    return ev;
}

std::vector<double> icm_equal_chop_payouts(const std::vector<double>& payouts) {
    if (payouts.empty()) {
        throw std::invalid_argument("payouts must be non-empty");
    }
    double total = 0.0;
    for (double p : payouts) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("payouts must be finite and non-negative");
        }
        total += p;
    }
    const double each = total / static_cast<double>(payouts.size());
    return std::vector<double>(payouts.size(), each);
}

std::vector<double> icm_chop_surplus_vs_equal_split(const std::vector<double>& stacks,
                                                  const std::vector<double>& payouts) {
    const std::vector<double> icm = icm_expected_payouts(stacks, payouts);
    const std::vector<double> eq = icm_equal_chop_payouts(payouts);
    std::vector<double> out(icm.size());
    for (std::size_t i = 0; i < icm.size(); ++i) {
        out[i] = icm[i] - eq[i];
    }
    return out;
}

double icm_total_prize_pool(const std::vector<double>& payouts) {
    double total = 0.0;
    for (double p : payouts) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("payouts must be finite and non-negative");
        }
        total += p;
    }
    return total;
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
    double sum = 0.0;
    for (double p : sorted) {
        sum += p;
    }
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
    std::size_t leader = 0;
    for (std::size_t i = 1; i < stacks.size(); ++i) {
        if (stacks[i] > stacks[leader]) {
            leader = i;
        }
    }
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
