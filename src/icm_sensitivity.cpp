#include "poker/icm_sensitivity.hpp"

#include "poker/icm.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace poker {

std::vector<double> icm_harville_stack_jacobian(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    if (n == 0 || payouts.size() != n) {
        throw std::invalid_argument("icmHarvilleStackJacobian: stacks and payouts must match");
    }
    for (double s : stacks) {
        if (!std::isfinite(s) || !(s > 0.0)) {
            throw std::invalid_argument("icmHarvilleStackJacobian: stacks must be finite and positive");
        }
    }
    std::vector<double> j(n * n, 0.0);
    // Central differences with a step relative to the stack (ICM only depends on stack ratios, so
    // the derivative scales as 1 / stack): h = eps^(1/3) * s balances truncation and rounding error,
    // keeps s - h positive, and the difference of the two bumped stacks is the exact step used.
    const double rel = std::cbrt(std::numeric_limits<double>::epsilon());
    for (std::size_t col = 0; col < n; ++col) {
        auto up = stacks;
        auto down = stacks;
        up[col] = stacks[col] * (1.0 + rel);
        down[col] = stacks[col] * (1.0 - rel);
        const double step = up[col] - down[col];
        const auto ev_up = icm_expected_payouts(up, payouts);
        const auto ev_down = icm_expected_payouts(down, payouts);
        for (std::size_t row = 0; row < n; ++row) {
            j[row * n + col] = (ev_up[row] - ev_down[row]) / step;
        }
    }
    return j;
}

IcmChopNegotiationResult icm_chop_negotiation_analysis(const std::vector<double>& stacks,
                                                       const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    if (n == 0 || payouts.size() != n) {
        throw std::invalid_argument("icmChopNegotiationAnalysis: stacks and payouts must match");
    }
    double sum_s = 0.0;
    double prize = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum_s += stacks[i];
        prize += payouts[i];
    }
    if (sum_s <= 0.0) {
        throw std::invalid_argument("icmChopNegotiationAnalysis: positive chip sum required");
    }
    IcmChopNegotiationResult out;
    out.total_prize_pool = prize;
    out.chip_chop.resize(n);
    out.icm = icm_expected_payouts(stacks, payouts);
    out.surplus.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.chip_chop[i] = (stacks[i] / sum_s) * prize;
        out.surplus[i] = out.chip_chop[i] - out.icm[i];
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const double room_i = out.surplus[i];
            const double room_j = out.surplus[j];
            if (room_i > 0.0 && room_j < 0.0) {
                const double t = std::min(room_i, -room_j);
                if (t > 1e-9) {
                    out.pareto_pairs.push_back({i, j, t});
                }
            } else if (room_j > 0.0 && room_i < 0.0) {
                const double t = std::min(room_j, -room_i);
                if (t > 1e-9) {
                    out.pareto_pairs.push_back({j, i, t});
                }
            }
        }
    }
    return out;
}

}  // namespace poker
