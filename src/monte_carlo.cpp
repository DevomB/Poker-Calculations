#include "poker/monte_carlo.hpp"

#include "poker/fast_evaluator.hpp"

#include <algorithm>
#include <future>
#include <random>
#include <vector>

namespace poker {

namespace {

double hero_showdown_equity_strengths(const std::vector<std::uint64_t>& strengths) {
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

void collect_known(const std::vector<Card>& hero, const std::vector<Card>& board,
                   std::vector<bool>& used) {
    for (const auto& c : hero) {
        const int id = static_cast<int>(c.suit()) * 13 + static_cast<int>(c.rank());
        used[static_cast<std::size_t>(id)] = true;
    }
    for (const auto& c : board) {
        const int id = static_cast<int>(c.suit()) * 13 + static_cast<int>(c.rank());
        used[static_cast<std::size_t>(id)] = true;
    }
}

std::vector<Card> remaining_deck(const std::vector<bool>& used) {
    std::vector<Card> d;
    d.reserve(52);
    for (int s = 0; s < 4; ++s) {
        for (int r = 0; r < 13; ++r) {
            const int id = s * 13 + r;
            if (!used[static_cast<std::size_t>(id)]) {
                d.emplace_back(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(s));
            }
        }
    }
    return d;
}

template <typename Rng>
float accumulate_showdown_equity(Rng& rng, const std::vector<Card>& player_hand,
                                   const std::vector<Card>& community_cards, int iterations,
                                   int villains) {
    if (iterations <= 0) {
        return 0.0F;
    }
    if (villains < 1) {
        villains = 1;
    }
    std::vector<bool> known(52, false);
    collect_known(player_hand, community_cards, known);

    std::vector<Card> pool = remaining_deck(known);
    std::vector<std::vector<Card>> villain_holes(static_cast<std::size_t>(villains));
    for (auto& vh : villain_holes) {
        vh.reserve(2);
    }

    double equity_sum = 0.0;
    std::vector<std::uint64_t> strengths;
    strengths.reserve(static_cast<std::size_t>(villains) + 1);
    std::vector<Card> runout;
    runout.reserve(5);
    std::uint8_t ranks[7]{};
    std::uint8_t suits[7]{};

    for (int i = 0; i < iterations; ++i) {
        std::shuffle(pool.begin(), pool.end(), rng);

        std::size_t idx = 0;
        for (auto& vh : villain_holes) {
            vh.clear();
            vh.push_back(pool[idx++]);
            vh.push_back(pool[idx++]);
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
        equity_sum += hero_showdown_equity_strengths(strengths);
    }
    return static_cast<float>(equity_sum / static_cast<double>(iterations));
}

float run_chunk(const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
                int count, std::uint32_t seed, int villains) {
    std::mt19937 rng(seed);
    return accumulate_showdown_equity(rng, player_hand, community_cards, count, villains);
}

}  // namespace

float simulate_hand_outcome(const std::vector<Card>& player_hand,
                            const std::vector<Card>& community_cards, int num_simulations,
                            std::mt19937& rng, int villains) {
    return accumulate_showdown_equity(rng, player_hand, community_cards, num_simulations,
                                      villains);
}

float parallel_hand_simulation(const std::vector<Card>& player_hand,
                               const std::vector<Card>& community_cards, int num_simulations,
                               std::uint32_t base_seed, int villains, std::size_t num_threads) {
    if (num_simulations <= 0) {
        return 0.0F;
    }
    if (num_threads == 0) {
        num_threads = 1;
    }
    num_threads = std::min<std::size_t>(num_threads, static_cast<std::size_t>(num_simulations));
    const int base = num_simulations / static_cast<int>(num_threads);
    const int rem = num_simulations % static_cast<int>(num_threads);

    std::vector<std::future<float>> futures;
    futures.reserve(num_threads);
    for (std::size_t t = 0; t < num_threads; ++t) {
        const int chunk = base + (static_cast<int>(t) < rem ? 1 : 0);
        const std::uint32_t seed = base_seed + static_cast<std::uint32_t>(t) * 9743U;
        futures.push_back(std::async(std::launch::async, [&, chunk, seed]() {
            return run_chunk(player_hand, community_cards, chunk, seed, villains);
        }));
    }
    double weighted = 0.0;
    int total = 0;
    for (std::size_t t = 0; t < num_threads; ++t) {
        const int chunk = base + (static_cast<int>(t) < rem ? 1 : 0);
        weighted += static_cast<double>(futures[t].get()) * static_cast<double>(chunk);
        total += chunk;
    }
    if (total <= 0) {
        return 0.0F;
    }
    return static_cast<float>(weighted / static_cast<double>(total));
}

void simulate_hand_outcome_batch(const std::vector<SimSpot>& spots, std::vector<float>& out_equities) {
    out_equities.resize(spots.size());
    for (std::size_t i = 0; i < spots.size(); ++i) {
        const SimSpot& s = spots[static_cast<std::size_t>(i)];
        std::mt19937 rng(s.seed);
        out_equities[i] =
            simulate_hand_outcome(s.hole, s.board, s.num_simulations, rng, s.villains);
    }
}

}  // namespace poker
