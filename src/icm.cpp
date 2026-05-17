#include "poker/icm.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace poker {

namespace {

[[nodiscard]] int popcount32(std::uint32_t x) {
    int c = 0;
    while (x != 0) {
        ++c;
        x &= x - 1U;
    }
    return c;
}

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
    const int alive = popcount32(mask);
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

[[nodiscard]] std::vector<std::vector<double>> harville_placement_matrix(
    const std::vector<double>& stacks) {
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
    std::vector<std::vector<double>> placement(
        n, std::vector<double>(n, 0.0));
    harville_placement_recur(stacks, mask, 1, 1.0, placement);
    return placement;
}

}  // namespace

std::vector<double> icm_win_probabilities_harville(const std::vector<double>& stacks) {
    const auto p = harville_placement_matrix(stacks);
    const std::size_t n = stacks.size();
    std::vector<double> win(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        win[i] = p[i][0];
    }
    return win;
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
    const auto placement = harville_placement_matrix(stacks);
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

}  // namespace poker
