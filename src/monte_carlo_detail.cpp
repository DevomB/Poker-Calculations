#include "poker/monte_carlo_detail.hpp"

#include "poker/cancel.hpp"
#include "poker/card_string.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace poker {

namespace {

constexpr int kCancelCheckInterval = 4096;

double hero_share_from_strengths(const std::vector<std::uint64_t>& strengths) {
    if (strengths.empty()) {
        return 0.0;
    }
    if (strengths.size() == 1) {
        return 1.0;
    }
    std::uint64_t best = strengths[0];
    int tied_at_best = 1;
    bool hero_tied = true;
    for (std::size_t i = 1; i < strengths.size(); ++i) {
        const std::uint64_t s = strengths[i];
        if (best < s) {
            best = s;
            tied_at_best = 1;
            hero_tied = false;
        } else if (s == best) {
            ++tied_at_best;
        }
    }
    if (!hero_tied || tied_at_best <= 0) {
        return 0.0;
    }
    return 1.0 / static_cast<double>(tied_at_best);
}

}  // namespace

McEquityDetailedResult simulate_hand_outcome_detailed(
    const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
    int num_simulations, std::mt19937& rng, int villains, const CancelPredicate* cancel) {
    McEquityDetailedResult out{};
    if (num_simulations <= 0) {
        return out;
    }
    if (villains < 1) {
        villains = 1;
    }
    out.n = num_simulations;
    double mean = 0.0;
    double m2 = 0.0;
    int count = 0;

    std::vector<bool> known(52, false);
    for (const auto& c : player_hand) {
        known[static_cast<std::size_t>(deck_index_from_card(c))] = true;
    }
    for (const auto& c : community_cards) {
        known[static_cast<std::size_t>(deck_index_from_card(c))] = true;
    }
    std::vector<Card> pool;
    pool.reserve(52);
    for (int i = 0; i < 52; ++i) {
        if (!known[static_cast<std::size_t>(i)]) {
            pool.push_back(card_from_deck_index(i));
        }
    }
    std::vector<std::array<Card, 2>> villain_holes(static_cast<std::size_t>(villains));
    std::vector<Card> runout;
    runout.reserve(5);
    std::uint8_t ranks[7]{};
    std::uint8_t suits[7]{};

    for (int iter = 0; iter < num_simulations; ++iter) {
        if (iter > 0 && (iter % kCancelCheckInterval) == 0) {
            throw_if_cancelled(cancel);
        }
        std::shuffle(pool.begin(), pool.end(), rng);
        std::size_t idx = 0;
        for (auto& vh : villain_holes) {
            vh[0] = pool[idx++];
            vh[1] = pool[idx++];
        }
        runout.assign(community_cards.begin(), community_cards.end());
        while (runout.size() < 5) {
            runout.push_back(pool[idx++]);
        }
        std::vector<std::uint64_t> strengths;
        strengths.reserve(static_cast<std::size_t>(villains) + 1);
        ranks[0] = player_hand[0].rank();
        suits[0] = player_hand[0].suit();
        ranks[1] = player_hand[1].rank();
        suits[1] = player_hand[1].suit();
        for (std::size_t b = 0; b < runout.size(); ++b) {
            ranks[2 + b] = runout[b].rank();
            suits[2 + b] = runout[b].suit();
        }
        strengths.push_back(evaluate_seven_strength_fast(ranks, suits));
        for (const auto& vh : villain_holes) {
            ranks[0] = vh[0].rank();
            suits[0] = vh[0].suit();
            ranks[1] = vh[1].rank();
            suits[1] = vh[1].suit();
            strengths.push_back(evaluate_seven_strength_fast(ranks, suits));
        }
        const double x = hero_share_from_strengths(strengths);
        ++count;
        const double delta = x - mean;
        mean += delta / static_cast<double>(count);
        const double delta2 = x - mean;
        m2 += delta * delta2;
    }

    out.estimate = mean;
    out.se = monte_carlo_standard_error(mean, count);
    const int successes = static_cast<int>(std::lround(mean * static_cast<double>(count)));
    const Wilson_interval w = wilson_score_interval(successes, count, 1.96);
    out.ci_low = w.lower;
    out.ci_high = w.upper;
    return out;
}

float simulate_equity_vs_range(const std::vector<Card>& player_hand,
                               const std::vector<Card>& community_cards,
                               const SparseRange& villain_range, int num_simulations,
                               std::mt19937& rng, const CancelPredicate* cancel) {
    if (num_simulations <= 0 || villain_range.combos.empty()) {
        return 0.0F;
    }
    double sum = 0.0;
    for (int i = 0; i < num_simulations; ++i) {
        if (i > 0 && (i % kCancelCheckInterval) == 0) {
            throw_if_cancelled(cancel);
        }
        std::uniform_real_distribution<double> dist(0.0, villain_range.weight_sum);
        const double pick = dist(rng);
        double acc = 0.0;
        const WeightedHoleCombo* chosen = &villain_range.combos.back();
        for (const WeightedHoleCombo& c : villain_range.combos) {
            acc += c.weight;
            if (pick <= acc) {
                chosen = &c;
                break;
            }
        }
        sum += static_cast<double>(simulate_hand_outcome_vs_villain_holes(
            player_hand, community_cards, chosen->card_a, chosen->card_b, 1, rng, cancel));
    }
    return static_cast<float>(sum / static_cast<double>(num_simulations));
}

}  // namespace poker
