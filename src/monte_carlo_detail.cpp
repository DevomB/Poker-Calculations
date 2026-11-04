#include "poker/monte_carlo_detail.hpp"

#include "poker/cancel.hpp"
#include "poker/card_string.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace poker {

namespace {

constexpr int kCancelCheckInterval = 4096;

/// Welford running mean / sum of squared deviations of per-trial payoffs in [0, 1].
struct PayoffMoments {
    int count{0};
    double mean{0.0};
    double m2{0.0};

    void add(double x) {
        ++count;
        const double delta = x - mean;
        mean += delta / static_cast<double>(count);
        m2 += delta * (x - mean);
    }

    // Each trial pays hero a share in [0, 1] (1 / k on a k-way chop), so trials are not Bernoulli.
    // `se` is the plug-in standard error from the sampled payoff variance m2 / (n - 1). One draw has
    // no sample variance; report 0.5, the largest standard deviation a [0, 1] payoff can have.
    // The interval does not use the sample variance: an all-equal sample (variance 0) says little
    // about rare outcomes. It is the Wilson score interval at the unrounded mean, which is valid
    // (conservative) for any payoff in [0, 1] because Var <= mu (1 - mu).
    [[nodiscard]] McEquityDetailedResult summary() const {
        McEquityDetailedResult out{};
        const double n = static_cast<double>(count);
        out.n = count;
        out.estimate = mean;
        out.se = count > 1 ? std::sqrt(std::max(0.0, m2 / (n - 1.0)) / n) : 0.5;
        const Wilson_interval w = wilson_score_interval_for_mean(std::clamp(mean, 0.0, 1.0), n, 1.96);
        out.ci_low = w.lower;
        out.ci_high = w.upper;
        return out;
    }
};

}  // namespace

McEquityDetailedResult simulate_hand_outcome_detailed(
    const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
    int num_simulations, std::mt19937& rng, int villains, const CancelPredicate* cancel) {
    if (num_simulations < 1) {
        throw std::invalid_argument("numSimulations must be at least 1");
    }
    if (villains < 1) {
        villains = 1;
    }
    validate_holdem_spot(player_hand, community_cards, villains);
    PayoffMoments moments;

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
    std::vector<std::uint64_t> strengths;
    strengths.reserve(static_cast<std::size_t>(villains) + 1);

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
        strengths.clear();
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
        moments.add(hero_showdown_share(strengths));
    }

    return moments.summary();
}

McRangesEquityResult simulate_equity_vs_ranges(const std::vector<Card>& player_hand,
                                               const std::vector<Card>& community_cards,
                                               const std::vector<SparseRange>& villain_ranges,
                                               const std::vector<Card>& dead_cards, int num_simulations,
                                               std::int64_t max_attempts, std::mt19937& rng,
                                               const CancelPredicate* cancel) {
    if (num_simulations < 1) {
        throw std::invalid_argument("numSimulations must be at least 1");
    }
    const std::size_t k = villain_ranges.size();
    if (k < 1 || k > 9) {
        throw std::invalid_argument("ranges must have 1 to 9 entries");
    }
    validate_holdem_spot(player_hand, community_cards, static_cast<int>(k));
    DeckBitset known;
    known.mark_cards_distinct(player_hand, "hero, board, or dead cards");
    known.mark_cards_distinct(community_cards, "hero, board, or dead cards");
    known.mark_cards_distinct(dead_cards, "hero, board, or dead cards");
    const int need_board = 5 - static_cast<int>(community_cards.size());
    if (52 - known.count() < 2 * static_cast<int>(k) + need_board) {
        throw std::invalid_argument("not enough cards left for every opponent and the board");
    }

    // Per range: live combos and cumulative weights for inverse-CDF draws.
    struct LiveRange {
        std::vector<const WeightedHoleCombo*> combos;
        std::vector<double> cumulative;
    };
    std::vector<LiveRange> live(k);
    for (std::size_t i = 0; i < k; ++i) {
        double acc = 0.0;
        for (const WeightedHoleCombo& c : villain_ranges[i].combos) {
            if (!(c.weight > 0.0) || known.test(c.card_a) || known.test(c.card_b)) {
                continue;
            }
            acc += c.weight;
            live[i].combos.push_back(&c);
            live[i].cumulative.push_back(acc);
        }
        if (live[i].combos.empty()) {
            throw std::invalid_argument("ranges[" + std::to_string(i) + "] has no combo left after blockers");
        }
    }
    max_attempts = std::max<std::int64_t>(max_attempts, num_simulations);

    McRangesEquityResult out{};
    PayoffMoments moments;
    std::vector<const WeightedHoleCombo*> picked(k);
    std::vector<int> pool;
    pool.reserve(52);
    std::vector<std::uint64_t> strengths(k + 1);
    std::uint8_t ranks[7]{};
    std::uint8_t suits[7]{};
    std::array<int, 5> board{};
    for (std::size_t b = 0; b < community_cards.size(); ++b) {
        board[b] = deck_index_from_card(community_cards[b]);
    }

    while (moments.count < num_simulations) {
        // With a budget of A draws the sampler can only deliver n tuples when the acceptance rate is
        // at least about n / A; below that it stops with an error instead of running unbounded.
        if (out.attempts >= max_attempts) {
            throw std::invalid_argument(
                "only " + std::to_string(moments.count) + " of " + std::to_string(num_simulations) +
                " opponent-hand draws were card-disjoint in " + std::to_string(out.attempts) +
                " attempts: the ranges overlap too much (raise maxAttempts or widen them)");
        }
        ++out.attempts;
        if ((out.attempts % kCancelCheckInterval) == 0) {
            throw_if_cancelled(cancel);
        }
        // Draw the whole tuple and start over from the first range on any shared card. Redrawing
        // only the colliding range would favour hands that rarely collide and skew the joint law.
        std::uint64_t used = known.mask;
        bool disjoint = true;
        for (std::size_t i = 0; i < k && disjoint; ++i) {
            const LiveRange& r = live[i];
            std::uniform_real_distribution<double> u(0.0, r.cumulative.back());
            const auto it = std::upper_bound(r.cumulative.begin(), r.cumulative.end(), u(rng));
            const std::size_t at = std::min<std::size_t>(static_cast<std::size_t>(it - r.cumulative.begin()),
                                                         r.combos.size() - 1);
            const WeightedHoleCombo* c = r.combos[at];
            const std::uint64_t bits = (std::uint64_t{1} << c->card_a) | (std::uint64_t{1} << c->card_b);
            disjoint = (used & bits) == 0;
            used |= bits;
            picked[i] = c;
        }
        if (!disjoint) {
            continue;
        }
        pool.clear();
        for (int d = 0; d < 52; ++d) {
            if ((used & (std::uint64_t{1} << d)) == 0) {
                pool.push_back(d);
            }
        }
        for (int j = 0; j < need_board; ++j) {
            std::uniform_int_distribution<int> pick(j, static_cast<int>(pool.size()) - 1);
            std::swap(pool[static_cast<std::size_t>(j)], pool[static_cast<std::size_t>(pick(rng))]);
            board[community_cards.size() + static_cast<std::size_t>(j)] = pool[static_cast<std::size_t>(j)];
        }
        for (int b = 0; b < 5; ++b) {
            ranks[2 + b] = static_cast<std::uint8_t>(board[static_cast<std::size_t>(b)] / 4);
            suits[2 + b] = static_cast<std::uint8_t>(board[static_cast<std::size_t>(b)] % 4);
        }
        ranks[0] = player_hand[0].rank();
        suits[0] = player_hand[0].suit();
        ranks[1] = player_hand[1].rank();
        suits[1] = player_hand[1].suit();
        strengths[0] = evaluate_seven_strength_fast(ranks, suits);
        for (std::size_t i = 0; i < k; ++i) {
            ranks[0] = static_cast<std::uint8_t>(picked[i]->card_a / 4);
            suits[0] = static_cast<std::uint8_t>(picked[i]->card_a % 4);
            ranks[1] = static_cast<std::uint8_t>(picked[i]->card_b / 4);
            suits[1] = static_cast<std::uint8_t>(picked[i]->card_b % 4);
            strengths[i + 1] = evaluate_seven_strength_fast(ranks, suits);
        }
        moments.add(hero_showdown_share(strengths));
    }
    out.equity = moments.summary();
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
