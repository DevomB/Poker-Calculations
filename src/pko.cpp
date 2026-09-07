#include "poker/pko.hpp"

#include "poker/icm.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>

namespace poker {

namespace {

constexpr double kBustEps = 1e-12;

void require_finite_nonneg(const std::vector<double>& v, const char* name) {
    for (double x : v) {
        if (!std::isfinite(x) || x < 0.0) {
            throw std::invalid_argument(std::string(name) + " must be finite and non-negative");
        }
    }
}

void require_player_count(const std::vector<double>& stacks) {
    const std::size_t n = stacks.size();
    if (n < 2 || n > 31) {
        throw std::invalid_argument("PKO: need 2..31 players");
    }
    require_finite_nonneg(stacks, "stacks");
}

void require_aligned(const std::vector<double>& stacks, const std::vector<double>& other, const char* name) {
    if (other.size() != stacks.size()) {
        throw std::invalid_argument(std::string(name) + " length must match stacks");
    }
    require_finite_nonneg(other, name);
}

[[nodiscard]] double stack_sum(const std::vector<double>& stacks) {
    return std::accumulate(stacks.begin(), stacks.end(), 0.0);
}

[[nodiscard]] std::vector<double> last_place_alive(const std::vector<double>& stacks) {
    const std::size_t n = stacks.size();
    std::vector<double> out(n, 0.0);
    std::vector<std::size_t> alive;
    std::vector<double> alive_stacks;
    alive.reserve(n);
    alive_stacks.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] > 0.0) {
            alive.push_back(i);
            alive_stacks.push_back(stacks[i]);
        }
    }
    if (alive.size() < 2) {
        return out;
    }
    const auto lp = icm_last_place_probabilities_harville(alive_stacks);
    for (std::size_t k = 0; k < alive.size(); ++k) {
        out[alive[k]] = lp[k];
    }
    return out;
}

/// ICM on remaining chips: busted seats (stack == 0) split the last-k prizes equally.
[[nodiscard]] std::vector<double> icm_allowing_busts(const std::vector<double>& stacks,
                                                     const std::vector<double>& payouts) {
    const std::size_t n = stacks.size();
    if (payouts.size() != n) {
        throw std::invalid_argument("PKO: payouts length must match stacks");
    }
    require_finite_nonneg(payouts, "payouts");

    std::vector<std::size_t> alive;
    std::vector<std::size_t> dead;
    alive.reserve(n);
    dead.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (stacks[i] > kBustEps) {
            alive.push_back(i);
        } else {
            dead.push_back(i);
        }
    }

    std::vector<double> ev(n, 0.0);
    const std::size_t k = dead.size();
    if (k > 0) {
        double dead_prize = 0.0;
        for (std::size_t r = n - k; r < n; ++r) {
            dead_prize += payouts[r];
        }
        const double each = dead_prize / static_cast<double>(k);
        for (std::size_t i : dead) {
            ev[i] = each;
        }
    }
    if (alive.empty()) {
        return ev;
    }
    std::vector<double> alive_stacks;
    std::vector<double> alive_payouts;
    alive_stacks.reserve(alive.size());
    alive_payouts.reserve(alive.size());
    for (std::size_t i : alive) {
        alive_stacks.push_back(stacks[i]);
    }
    for (std::size_t r = 0; r < alive.size(); ++r) {
        alive_payouts.push_back(payouts[r]);
    }
    const auto alive_ev = icm_expected_payouts(alive_stacks, alive_payouts);
    for (std::size_t t = 0; t < alive.size(); ++t) {
        ev[alive[t]] = alive_ev[t];
    }
    return ev;
}

[[nodiscard]] bool covers(double hunter, double prey) {
    return hunter + 1e-15 >= prey && prey > 0.0;
}

void require_seat(std::size_t idx, std::size_t n, const char* name) {
    if (idx >= n) {
        throw std::invalid_argument(std::string(name) + " index out of range");
    }
}

void normalize_outcomes(double& p_win, double& p_tie, double& p_lose) {
    if (!std::isfinite(p_win) || !std::isfinite(p_tie) || !std::isfinite(p_lose) || p_win < 0.0 ||
        p_tie < 0.0 || p_lose < 0.0) {
        throw std::invalid_argument("PKO: outcome probabilities must be finite and non-negative");
    }
    const double s = p_win + p_tie + p_lose;
    if (s <= 0.0) {
        throw std::invalid_argument("PKO: outcome probabilities must sum to a positive value");
    }
    p_win /= s;
    p_tie /= s;
    p_lose /= s;
}

struct HandStacks {
    std::vector<double> stacks;
    double hero_bounty = 0.0;
};

[[nodiscard]] HandStacks resolve_all_in(const std::vector<double>& stacks, std::size_t hero,
                                        std::size_t villain, double pot, int winner,
                                        const std::vector<double>& bounty_values) {
    // winner: 0 hero, 1 villain, 2 tie
    const double hero_stk = stacks[hero];
    const double vil_stk = stacks[villain];
    const double effective = std::min(hero_stk, vil_stk);
    HandStacks out;
    out.stacks = stacks;
    out.stacks[hero] = hero_stk - effective;
    out.stacks[villain] = vil_stk - effective;
    const double pot_final = pot + 2.0 * effective;
    if (winner == 2) {
        out.stacks[hero] += 0.5 * pot_final;
        out.stacks[villain] += 0.5 * pot_final;
        return out;
    }
    if (winner == 0) {
        out.stacks[hero] += pot_final;
        if (out.stacks[villain] <= kBustEps) {
            out.stacks[villain] = 0.0;
            out.hero_bounty = bounty_values[villain];
        }
        return out;
    }
    out.stacks[villain] += pot_final;
    if (out.stacks[hero] <= kBustEps) {
        out.stacks[hero] = 0.0;
    }
    return out;
}

[[nodiscard]] double seat_dollar_ev(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                    const std::vector<double>& bounty_values, std::size_t seat,
                                    double immediate_bounty) {
    const auto icm = icm_allowing_busts(stacks, payouts);
    const auto bounty = pko_expected_bounty_collection(stacks, bounty_values);
    return icm[seat] + bounty[seat] + immediate_bounty;
}

}  // namespace

PkoKnockoutMatrix pko_knockout_probability_matrix(const std::vector<double>& stacks) {
    require_player_count(stacks);
    const std::size_t n = stacks.size();
    PkoKnockoutMatrix out;
    out.n = n;
    out.flat.assign(n * n, 0.0);

    const double total = stack_sum(stacks);
    if (total <= 0.0) {
        return out;
    }
    const auto p_bust = last_place_alive(stacks);
    for (std::size_t j = 0; j < n; ++j) {
        if (stacks[j] <= 0.0 || p_bust[j] <= 0.0) {
            continue;
        }
        const double denom = total - stacks[j];
        if (denom <= 0.0) {
            continue;
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == j) {
                continue;
            }
            if (!covers(stacks[i], stacks[j])) {
                continue;
            }
            out.flat[i * n + j] = p_bust[j] * (stacks[i] / denom);
        }
    }
    return out;
}

std::vector<double> pko_expected_bounty_collection(const std::vector<double>& stacks,
                                                   const std::vector<double>& bounty_values) {
    require_player_count(stacks);
    require_aligned(stacks, bounty_values, "bountyValues");
    const auto mat = pko_knockout_probability_matrix(stacks);
    const std::size_t n = stacks.size();
    std::vector<double> ev(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            ev[i] += bounty_values[j] * mat.flat[i * n + j];
        }
    }
    return ev;
}

PkoIcmbuResult pko_icmbu_payouts(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                 const std::vector<double>& bounty_values) {
    require_player_count(stacks);
    require_aligned(stacks, payouts, "payouts");
    require_aligned(stacks, bounty_values, "bountyValues");
    PkoIcmbuResult r;
    r.icm = icm_allowing_busts(stacks, payouts);
    r.bounty = pko_expected_bounty_collection(stacks, bounty_values);
    r.icmbu.resize(stacks.size());
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        r.icmbu[i] = r.icm[i] + r.bounty[i];
    }
    return r;
}

PkoBountyRiskPremiumResult pko_bounty_risk_premium(const std::vector<double>& stacks,
                                                   const std::vector<double>& payouts,
                                                   const std::vector<double>& bounty_values) {
    const auto icmbu = pko_icmbu_payouts(stacks, payouts, bounty_values);
    PkoBountyRiskPremiumResult r;
    r.freezeout_icm = icmbu.icm;
    r.icmbu = icmbu.icmbu;
    r.icmbu_minus_freezeout = icmbu.bounty;
    const std::size_t n = stacks.size();
    const double total = stack_sum(stacks);
    const double pool = std::accumulate(bounty_values.begin(), bounty_values.end(), 0.0);
    r.chip_share_bounty_ev.assign(n, 0.0);
    r.bounty_risk_premium.assign(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        r.chip_share_bounty_ev[i] = (total > 0.0) ? (stacks[i] / total) * pool : 0.0;
        r.bounty_risk_premium[i] = r.chip_share_bounty_ev[i] - icmbu.bounty[i];
    }
    return r;
}

PkoSpotEvResult pko_call_ev_vs_shove(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                     const std::vector<double>& bounty_values, std::size_t hero,
                                     std::size_t villain, double pot, double p_win, double p_tie,
                                     double p_lose) {
    require_player_count(stacks);
    require_aligned(stacks, payouts, "payouts");
    require_aligned(stacks, bounty_values, "bountyValues");
    require_seat(hero, stacks.size(), "hero");
    require_seat(villain, stacks.size(), "villain");
    if (hero == villain) {
        throw std::invalid_argument("PKO: hero and villain must differ");
    }
    if (!std::isfinite(pot) || pot < 0.0) {
        throw std::invalid_argument("PKO: pot must be finite and non-negative");
    }
    normalize_outcomes(p_win, p_tie, p_lose);

    PkoSpotEvResult r;
    r.fold_ev = seat_dollar_ev(stacks, payouts, bounty_values, hero, 0.0);

    const auto win = resolve_all_in(stacks, hero, villain, pot, 0, bounty_values);
    const auto lose = resolve_all_in(stacks, hero, villain, pot, 1, bounty_values);
    const auto tie = resolve_all_in(stacks, hero, villain, pot, 2, bounty_values);
    const double ev_win = seat_dollar_ev(win.stacks, payouts, bounty_values, hero, win.hero_bounty);
    const double ev_lose = seat_dollar_ev(lose.stacks, payouts, bounty_values, hero, lose.hero_bounty);
    const double ev_tie = seat_dollar_ev(tie.stacks, payouts, bounty_values, hero, tie.hero_bounty);
    r.take_ev = p_win * ev_win + p_lose * ev_lose + p_tie * ev_tie;
    r.delta = r.take_ev - r.fold_ev;
    return r;
}

PkoSpotEvResult pko_jam_ev_vs_fold(const std::vector<double>& stacks, const std::vector<double>& payouts,
                                   const std::vector<double>& bounty_values, std::size_t hero,
                                   std::size_t villain, double pot, double fold_equity, double p_win,
                                   double p_tie, double p_lose) {
    require_player_count(stacks);
    require_aligned(stacks, payouts, "payouts");
    require_aligned(stacks, bounty_values, "bountyValues");
    require_seat(hero, stacks.size(), "hero");
    require_seat(villain, stacks.size(), "villain");
    if (hero == villain) {
        throw std::invalid_argument("PKO: hero and villain must differ");
    }
    if (!std::isfinite(pot) || pot < 0.0) {
        throw std::invalid_argument("PKO: pot must be finite and non-negative");
    }
    if (!std::isfinite(fold_equity) || fold_equity < 0.0 || fold_equity > 1.0) {
        throw std::invalid_argument("PKO: foldEquity must be in [0,1]");
    }
    normalize_outcomes(p_win, p_tie, p_lose);

    PkoSpotEvResult r;
    r.fold_ev = seat_dollar_ev(stacks, payouts, bounty_values, hero, 0.0);

    auto folded_stacks = stacks;
    folded_stacks[hero] += pot;
    const double ev_fold_out = seat_dollar_ev(folded_stacks, payouts, bounty_values, hero, 0.0);

    const auto called = pko_call_ev_vs_shove(stacks, payouts, bounty_values, hero, villain, pot, p_win,
                                             p_tie, p_lose);
    r.take_ev = fold_equity * ev_fold_out + (1.0 - fold_equity) * called.take_ev;
    r.delta = r.take_ev - r.fold_ev;
    return r;
}

MysteryBountyEvResult mystery_bounty_expected_value(const std::vector<double>& values,
                                                    const std::vector<double>& weights, int k) {
    if (values.empty()) {
        throw std::invalid_argument("mystery bounty: values must be non-empty");
    }
    require_finite_nonneg(values, "values");
    std::vector<double> w = weights;
    if (w.empty()) {
        w.assign(values.size(), 1.0);
    }
    if (w.size() != values.size()) {
        throw std::invalid_argument("mystery bounty: weights length must match values");
    }
    require_finite_nonneg(w, "weights");
    const double wsum = std::accumulate(w.begin(), w.end(), 0.0);
    if (wsum <= 0.0) {
        throw std::invalid_argument("mystery bounty: positive weight sum required");
    }

    MysteryBountyEvResult r;
    r.all_remaining = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        mean += values[i] * (w[i] / wsum);
    }
    r.one_draw = mean;
    const int n = static_cast<int>(values.size());
    if (k < 0) {
        throw std::invalid_argument("mystery bounty: k must be non-negative");
    }
    r.k = k;
    if (k == 0) {
        r.sample_k = 0.0;
    } else if (k >= n) {
        r.sample_k = r.all_remaining;
        r.k = n;
    } else {
        // Equal-weight WOR: E[sum of k] = k * mean. Same linearity used as the
        // weighted with-replacement mean when weights differ (documented ceiling:
        // exact WOR inclusion for unequal weights is O(2^n); upgrade with DP).
        r.sample_k = static_cast<double>(k) * r.one_draw;
    }
    return r;
}

std::vector<double> progressive_ko_posted_bounty(const std::vector<double>& base_bounties,
                                                 const std::vector<double>& knockouts,
                                                 double carry_fraction) {
    const std::size_t n = base_bounties.size();
    if (n < 1) {
        throw std::invalid_argument("progressive KO: need at least one player");
    }
    require_finite_nonneg(base_bounties, "baseBounties");
    require_finite_nonneg(knockouts, "knockouts");
    if (!std::isfinite(carry_fraction) || carry_fraction < 0.0) {
        throw std::invalid_argument("progressive KO: carryFraction must be finite and non-negative");
    }

    std::vector<double> collected(n, 0.0);
    if (knockouts.size() == n) {
        collected = knockouts;
    } else if (knockouts.size() == n * n) {
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                if (i == j) {
                    continue;
                }
                collected[i] += knockouts[i * n + j] * base_bounties[j];
            }
        }
    } else {
        throw std::invalid_argument("progressive KO: knockouts must be length n or n*n");
    }

    std::vector<double> posted(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        posted[i] = base_bounties[i] + carry_fraction * collected[i];
    }
    return posted;
}

PkoCoveringHuntResult pko_covering_hunt_ev(const std::vector<double>& stacks,
                                           const std::vector<double>& payouts,
                                           const std::vector<double>& bounty_values, std::size_t hunter,
                                           std::size_t prey, double pot, double equity,
                                           bool equity_provided) {
    require_player_count(stacks);
    require_aligned(stacks, payouts, "payouts");
    require_aligned(stacks, bounty_values, "bountyValues");
    require_seat(hunter, stacks.size(), "hunter");
    require_seat(prey, stacks.size(), "prey");
    if (hunter == prey) {
        throw std::invalid_argument("PKO hunt: hunter and prey must differ");
    }
    if (!covers(stacks[hunter], stacks[prey])) {
        throw std::invalid_argument("PKO hunt: hunter must cover prey (stack_hunter >= stack_prey > 0)");
    }
    if (!std::isfinite(pot) || pot < 0.0) {
        throw std::invalid_argument("PKO: pot must be finite and non-negative");
    }

    double eq = equity;
    if (!equity_provided) {
        const double denom = stacks[hunter] + stacks[prey];
        eq = denom > 0.0 ? stacks[hunter] / denom : 0.5;
    }
    if (!std::isfinite(eq) || eq < 0.0 || eq > 1.0) {
        throw std::invalid_argument("PKO hunt: equity must be in [0,1]");
    }

    const auto spot = pko_call_ev_vs_shove(stacks, payouts, bounty_values, hunter, prey, pot, eq, 0.0,
                                           1.0 - eq);
    PkoCoveringHuntResult r;
    r.equity_used = eq;
    r.hunt_ev = spot.take_ev;
    r.check_down_ev = spot.fold_ev;
    r.delta = spot.delta;
    return r;
}

PkoWinnerTakeBountiesResult pko_winner_take_remaining_bounties(const std::vector<double>& stacks,
                                                               const std::vector<double>& payouts,
                                                               double remaining_bounty_pool) {
    require_player_count(stacks);
    require_aligned(stacks, payouts, "payouts");
    if (!std::isfinite(remaining_bounty_pool) || remaining_bounty_pool < 0.0) {
        throw std::invalid_argument("PKO: remainingBountyPool must be finite and non-negative");
    }

    PkoWinnerTakeBountiesResult r;
    r.adjusted_payouts = payouts;
    r.adjusted_payouts[0] += remaining_bounty_pool;
    r.ev = icm_allowing_busts(stacks, r.adjusted_payouts);

    std::vector<std::size_t> alive;
    std::vector<double> alive_stacks;
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i] > kBustEps) {
            alive.push_back(i);
            alive_stacks.push_back(stacks[i]);
        }
    }
    r.win_probabilities.assign(stacks.size(), 0.0);
    if (alive.size() == 1) {
        r.win_probabilities[alive[0]] = 1.0;
    } else if (alive.size() >= 2) {
        const auto w = icm_win_probabilities_harville(alive_stacks);
        for (std::size_t t = 0; t < alive.size(); ++t) {
            r.win_probabilities[alive[t]] = w[t];
        }
    }
    r.bounty_to_winner_ev.resize(stacks.size());
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        r.bounty_to_winner_ev[i] = r.win_probabilities[i] * remaining_bounty_pool;
    }
    return r;
}

}  // namespace poker
