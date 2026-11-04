#include "poker/tournament_duel.hpp"

#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace poker {

namespace {

void require_positive_finite(double x, const char* what) {
    if (!std::isfinite(x) || !(x > 0.0)) {
        throw std::invalid_argument(std::string("tournamentDuel: ") + what + " must be finite and positive");
    }
}

/// sinh(x) / x, accurate for small |x|.
double sinhc(double x) {
    if (std::abs(x) < 1e-4) {
        const double x2 = x * x;
        return 1.0 + x2 / 6.0 * (1.0 + x2 / 20.0);
    }
    return std::sinh(x) / x;
}

/// sinhc(x) - sinhc(y) for |x|, |y| <= 1 without cancellation: the series
/// sum_k (x^{2k} - y^{2k}) / (2k + 1)!, with x^2 - y^2 = (x - y)(x + y) factored out.
double sinhc_difference_small(double x_minus_y, double x_plus_y, double x, double y) {
    const double a = x * x;
    const double b = y * y;
    double s_k = 1.0;     // sum_{j<k} a^j b^(k-1-j)
    double b_pow = 1.0;   // b^(k-1)
    double fact = 6.0;    // (2k + 1)!
    double sum = 0.0;
    for (int k = 1; k <= 14; ++k) {
        sum += s_k / fact;
        b_pow *= b;
        s_k = a * s_k + b_pow;
        fact *= static_cast<double>((2 * k + 2) * (2 * k + 3));
    }
    return x_minus_y * x_plus_y * sum;
}

}  // namespace

TournamentDuelResult tournament_duel_absorption_probabilities(
    double hero_stack, double villain_stack, double win_probability_per_hand,
    double chips_per_all_in, double winner_prize) {
    require_positive_finite(hero_stack, "heroStack");
    require_positive_finite(villain_stack, "villainStack");
    require_positive_finite(chips_per_all_in, "chipsPerAllIn");
    if (!std::isfinite(win_probability_per_hand) || win_probability_per_hand < 0.0 ||
        win_probability_per_hand > 1.0) {
        throw std::invalid_argument("tournamentDuel: win probability must be in [0,1]");
    }
    if (!std::isfinite(winner_prize) || winner_prize < 0.0) {
        throw std::invalid_argument("tournamentDuel: winnerPrize must be finite and non-negative");
    }
    // Gambler's ruin: each all-in moves one unit of `chips_per_all_in`; hero busts after h net
    // losses and wins after v net wins (N = h + v). With r = q / p = e^L:
    //   P(win) = (1 - r^h) / (1 - r^N),   E[hands] = (h - N P) / (q - p).
    // r^N overflows for long uneven duels, so P is formed from exp / expm1 of negative arguments,
    // and near a fair game (|L| N small) h - N P is formed without cancellation (see below).
    const double h = std::max(1.0, std::ceil(hero_stack / chips_per_all_in));
    const double v = std::max(1.0, std::ceil(villain_stack / chips_per_all_in));
    if (!std::isfinite(h) || !std::isfinite(v)) {
        throw std::invalid_argument("tournamentDuel: stack / chipsPerAllIn is too large");
    }
    const double n = h + v;
    const double p = win_probability_per_hand;

    TournamentDuelResult out;
    if (p == 0.0) {
        out.hero_win_probability = 0.0;
        out.expected_hands = h;
    } else if (p == 1.0) {
        out.hero_win_probability = 1.0;
        out.expected_hands = v;
    } else if (p == 0.5) {
        out.hero_win_probability = h / n;
        out.expected_hands = h * v;
    } else {
        const double q_minus_p = 1.0 - 2.0 * p;   // exact for p >= 1/4, accurate otherwise
        const double l = std::log1p(q_minus_p / p);  // ln(q / p)
        const double s = 0.5 * l;
        if (std::abs(s) * n <= 1.0) {
            // r^k = e^{2sk}: P = e^{-sv} sinh(sh) / sinh(sN) = e^{-sv} (h/N) sinhc(sh) / sinhc(sN), and
            // h - N P = h [sinhc(sN) - sinhc(sh) + (1 - e^{-sv}) sinhc(sh)] / sinhc(sN).
            const double gn = sinhc(s * n);
            const double gh = sinhc(s * h);
            out.hero_win_probability = std::exp(-s * v) * (h / n) * gh / gn;
            const double diff = sinhc_difference_small(s * v, s * (n + h), s * n, s * h);
            const double h_minus_np = h * (diff - std::expm1(-s * v) * gh) / gn;
            out.expected_hands = h_minus_np / q_minus_p;
        } else {
            if (l > 0.0) {
                // Hero is the underdog: divide through by r^N so every power is at most 1.
                out.hero_win_probability = std::exp(-v * l) * std::expm1(-h * l) / std::expm1(-n * l);
            } else {
                out.hero_win_probability = std::expm1(h * l) / std::expm1(n * l);
            }
            out.expected_hands = (h - n * out.hero_win_probability) / q_minus_p;
        }
    }
    out.hero_prize_ev = out.hero_win_probability * winner_prize;
    return out;
}

}  // namespace poker
