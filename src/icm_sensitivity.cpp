#include "poker/icm_sensitivity.hpp"

#include "poker/icm.hpp"

#include <cmath>
#include <stdexcept>

namespace poker {

std::vector<double> icm_harville_stack_jacobian(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    if (n == 0 || payouts.size() != n) {
        throw std::invalid_argument("icmHarvilleStackJacobian: stacks and payouts must match");
    }
    std::vector<double> j(n * n, 0.0);
    const auto base = icm_expected_payouts(stacks, payouts);
    for (std::size_t col = 0; col < n; ++col) {
        const double eps = std::max(1.0, 1e-6 * stacks[col]);
        auto bumped = stacks;
        bumped[col] += eps;
        const auto ev = icm_expected_payouts(bumped, payouts);
        for (std::size_t row = 0; row < n; ++row) {
            j[row * n + col] = (ev[row] - base[row]) / eps;
        }
    }
    return j;
}

IcmFieldPressureResult icm_field_pressure_index(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts,
                                                std::size_t hero_index, double pot_chips) {
    const std::size_t n = stacks.size();
    if (hero_index >= n) {
        throw std::invalid_argument("icmFieldPressureIndex: invalid hero index");
    }
    IcmFieldPressureResult out;
    out.pairwise_bubble_factors.assign(n, 0.0);
    double denom = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        if (j == hero_index) {
            continue;
        }
        denom += stacks[j];
    }
    if (denom <= 0.0) {
        throw std::invalid_argument("icmFieldPressureIndex: no opposing stacks");
    }
    double psi = 0.0;
    double max_bf = -1.0;
    for (std::size_t j = 0; j < n; ++j) {
        if (j == hero_index) {
            continue;
        }
        double bf = 0.0;
        try {
            bf = icm_pairwise_bubble_factor(stacks, payouts, hero_index, j, pot_chips);
        } catch (const std::exception&) {
            bf = 0.0;
        }
        out.pairwise_bubble_factors[j] = bf;
        const double w = stacks[j] / denom;
        psi += w * bf / (1.0 + bf);
        if (bf > max_bf) {
            max_bf = bf;
            out.argmax_villain = j;
        }
    }
    out.index = psi;
    return out;
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
