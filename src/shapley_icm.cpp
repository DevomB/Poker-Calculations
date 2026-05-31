#include "poker/shapley_icm.hpp"

#include "poker/bit_utils.hpp"
#include "poker/icm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>

namespace poker {

namespace {

double coalition_value(const std::vector<double>& stacks, const std::vector<double>& payouts,
                       std::uint32_t mask) {
    std::vector<double> sub_stacks;
    std::vector<double> sub_pay;
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        if ((mask & (1U << static_cast<unsigned>(i))) != 0) {
            sub_stacks.push_back(stacks[i]);
            sub_pay.push_back(payouts[i]);
        }
    }
    if (sub_stacks.empty()) {
        return 0.0;
    }
    double prize = 0.0;
    for (double p : sub_pay) {
        prize += p;
    }
    const auto ev = icm_expected_payouts(sub_stacks, sub_pay);
    double sum = 0.0;
    for (double v : ev) {
        sum += v;
    }
    return sum;
}

void shapley_exact(const std::vector<double>& stacks, const std::vector<double>& payouts,
                   std::vector<double>& phi) {
    const std::size_t n = stacks.size();
    phi.assign(n, 0.0);
    const std::size_t subsets = std::size_t{1} << n;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t s = 0; s < subsets; ++s) {
            if ((s & (std::size_t{1} << i)) != 0) {
                continue;
            }
            const std::size_t s_with = s | (std::size_t{1} << i);
            const int s_size = popcount_u32(static_cast<std::uint32_t>(s));
            const int n_int = static_cast<int>(n);
            auto fact = [](int x) {
                double f = 1.0;
                for (int k = 2; k <= x; ++k) {
                    f *= static_cast<double>(k);
                }
                return f;
            };
            const double coeff =
                fact(s_size) * fact(n_int - s_size - 1) / fact(n_int);
            const double v_with = coalition_value(stacks, payouts, static_cast<std::uint32_t>(s_with));
            const double v_without = coalition_value(stacks, payouts, static_cast<std::uint32_t>(s));
            phi[i] += coeff * (v_with - v_without);
        }
    }
}

}  // namespace

ShapleyIcmResult icm_shapley_values(const std::vector<double>& stacks,
                                    const std::vector<double>& payouts, const char* method,
                                    std::size_t permutations) {
    const std::size_t n = stacks.size();
    if (n == 0 || payouts.size() != n) {
        throw std::invalid_argument("icmShapleyValues: stacks and payouts must match");
    }
    ShapleyIcmResult out;
    out.method = method ? method : "exact";
    if (out.method == "exact" && n <= 8) {
        shapley_exact(stacks, payouts, out.values);
        return out;
    }
    out.method = "monteCarlo";
    out.values.assign(n, 0.0);
    out.se.assign(n, 0.0);
    std::vector<double> sum(n, 0.0);
    std::vector<double> sumsq(n, 0.0);
    std::mt19937 rng(42);
    std::vector<std::size_t> perm(n);
    for (std::size_t i = 0; i < n; ++i) {
        perm[i] = i;
    }
    for (std::size_t sample = 0; sample < permutations; ++sample) {
        std::shuffle(perm.begin(), perm.end(), rng);
        std::uint32_t mask = 0;
        for (std::size_t pi = 0; pi < n; ++pi) {
            const std::size_t player = perm[pi];
            const double before = coalition_value(stacks, payouts, mask);
            mask |= 1U << static_cast<unsigned>(player);
            const double after = coalition_value(stacks, payouts, mask);
            const double marginal = after - before;
            sum[player] += marginal;
            sumsq[player] += marginal * marginal;
        }
    }
    const double inv = 1.0 / static_cast<double>(permutations);
    for (std::size_t i = 0; i < n; ++i) {
        out.values[i] = sum[i] * inv;
        const double mean = out.values[i];
        const double var = std::max(0.0, sumsq[i] * inv - mean * mean);
        out.se[i] = std::sqrt(var / static_cast<double>(permutations));
    }
    return out;
}

}  // namespace poker
