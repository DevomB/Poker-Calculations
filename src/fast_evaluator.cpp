#include "poker/fast_evaluator.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <vector>

namespace poker {

namespace {

constexpr std::array<std::array<int, 5>, 21> kSevenChooseFive = {{
    {{0, 1, 2, 3, 4}}, {{0, 1, 2, 3, 5}}, {{0, 1, 2, 3, 6}}, {{0, 1, 2, 4, 5}},
    {{0, 1, 2, 4, 6}}, {{0, 1, 2, 5, 6}}, {{0, 1, 3, 4, 5}}, {{0, 1, 3, 4, 6}},
    {{0, 1, 3, 5, 6}}, {{0, 1, 4, 5, 6}}, {{0, 2, 3, 4, 5}}, {{0, 2, 3, 4, 6}},
    {{0, 2, 3, 5, 6}}, {{0, 2, 4, 5, 6}}, {{0, 3, 4, 5, 6}}, {{1, 2, 3, 4, 5}},
    {{1, 2, 3, 4, 6}}, {{1, 2, 3, 5, 6}}, {{1, 2, 4, 5, 6}}, {{1, 3, 4, 5, 6}},
    {{2, 3, 4, 5, 6}},
}};

int straight_high_from_mask(const std::array<int, 13>& present) {
    if (present[12] && present[0] && present[1] && present[2] && present[3]) {
        return 3;
    }
    for (int high = 12; high >= 4; --high) {
        bool ok = true;
        for (int d = 0; d < 5; ++d) {
            if (!present[static_cast<std::size_t>(high - d)]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return high;
        }
    }
    return -1;
}

HandEvaluation evaluate_five_core(const std::uint8_t ranks[5], const std::uint8_t suits[5]) {
    HandEvaluation e{};
    std::array<int, 13> freq{};
    for (int i = 0; i < 5; ++i) {
        freq[static_cast<std::size_t>(ranks[i])]++;
    }
    std::array<int, 13> present{};
    for (int r = 0; r < 13; ++r) {
        present[static_cast<std::size_t>(r)] = freq[static_cast<std::size_t>(r)] > 0 ? 1 : 0;
    }

    std::array<std::pair<int, int>, 5> groups{};
    int group_count = 0;
    for (int r = 12; r >= 0; --r) {
        const int c = freq[static_cast<std::size_t>(r)];
        if (c > 0) {
            groups[static_cast<std::size_t>(group_count++)] = {r, c};
        }
    }

    const bool flush = suits[0] == suits[1] && suits[0] == suits[2] && suits[0] == suits[3] &&
                       suits[0] == suits[4];
    const int sh = straight_high_from_mask(present);

    if (flush && sh >= 0) {
        e.rank = (sh == 12) ? HandRank::RoyalFlush : HandRank::StraightFlush;
        e.kickers[0] = static_cast<std::uint8_t>(sh);
        return e;
    }

    if (group_count > 0 && groups[0].second == 4) {
        e.rank = HandRank::FourOfAKind;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (int gi = 1; gi < group_count && ki < 5; ++gi) {
            for (int t = 0; t < groups[static_cast<std::size_t>(gi)].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[static_cast<std::size_t>(gi)].first);
            }
        }
        return e;
    }

    if (group_count >= 2 && groups[0].second == 3 && groups[1].second == 2) {
        e.rank = HandRank::FullHouse;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        e.kickers[1] = static_cast<std::uint8_t>(groups[1].first);
        return e;
    }

    if (flush) {
        e.rank = HandRank::Flush;
        int ki = 0;
        for (int gi = 0; gi < group_count && ki < 5; ++gi) {
            for (int t = 0; t < groups[static_cast<std::size_t>(gi)].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[static_cast<std::size_t>(gi)].first);
            }
        }
        return e;
    }

    if (sh >= 0) {
        e.rank = HandRank::Straight;
        e.kickers[0] = static_cast<std::uint8_t>(sh);
        return e;
    }

    if (group_count > 0 && groups[0].second == 3) {
        e.rank = HandRank::ThreeOfAKind;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (int gi = 1; gi < group_count && ki < 5; ++gi) {
            for (int t = 0; t < groups[static_cast<std::size_t>(gi)].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[static_cast<std::size_t>(gi)].first);
            }
        }
        return e;
    }

    int pair_ranks[2];
    int pair_count = 0;
    for (int gi = 0; gi < group_count; ++gi) {
        if (groups[static_cast<std::size_t>(gi)].second == 2) {
            pair_ranks[pair_count++] = groups[static_cast<std::size_t>(gi)].first;
        }
    }
    if (pair_count >= 2) {
        e.rank = HandRank::TwoPair;
        e.kickers[0] = static_cast<std::uint8_t>(pair_ranks[0]);
        e.kickers[1] = static_cast<std::uint8_t>(pair_ranks[1]);
        int ki = 2;
        for (int gi = 0; gi < group_count && ki < 5; ++gi) {
            if (groups[static_cast<std::size_t>(gi)].second != 1) {
                continue;
            }
            e.kickers[static_cast<std::size_t>(ki++)] =
                static_cast<std::uint8_t>(groups[static_cast<std::size_t>(gi)].first);
        }
        return e;
    }

    if (group_count > 0 && groups[0].second == 2) {
        e.rank = HandRank::OnePair;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (int gi = 1; gi < group_count && ki < 5; ++gi) {
            for (int t = 0; t < groups[static_cast<std::size_t>(gi)].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[static_cast<std::size_t>(gi)].first);
            }
        }
        return e;
    }

    e.rank = HandRank::HighCard;
    int ki = 0;
    for (int gi = 0; gi < group_count && ki < 5; ++gi) {
        for (int t = 0; t < groups[static_cast<std::size_t>(gi)].second && ki < 5; ++t) {
            e.kickers[static_cast<std::size_t>(ki++)] =
                static_cast<std::uint8_t>(groups[static_cast<std::size_t>(gi)].first);
        }
    }
    return e;
}

HandEvaluation partial_high_card_fast(const std::uint8_t ranks[], int card_count) {
    HandEvaluation e{};
    e.rank = HandRank::HighCard;
    std::array<std::uint8_t, 7> sorted{};
    for (int i = 0; i < card_count; ++i) {
        sorted[static_cast<std::size_t>(i)] = ranks[i];
    }
    std::sort(sorted.begin(), sorted.begin() + card_count, std::greater<std::uint8_t>{});
    for (int ki = 0; ki < card_count && ki < 5; ++ki) {
        e.kickers[static_cast<std::size_t>(ki)] = sorted[static_cast<std::size_t>(ki)];
    }
    return e;
}

void fill_seven_from_cards(const std::vector<Card>& hole, const std::vector<Card>& board,
                           std::uint8_t ranks[7], std::uint8_t suits[7]) {
    int n = 0;
    for (const Card& c : hole) {
        ranks[n] = c.rank();
        suits[n] = c.suit();
        ++n;
    }
    for (const Card& c : board) {
        ranks[n] = c.rank();
        suits[n] = c.suit();
        ++n;
    }
}

}  // namespace

std::uint64_t pack_hand_strength(const HandEvaluation& evaluation) {
    std::uint64_t x = static_cast<std::uint64_t>(evaluation.rank) << 24;
    for (int i = 0; i < 5; ++i) {
        x |= static_cast<std::uint64_t>(evaluation.kickers[static_cast<std::size_t>(i)] & 0x1F)
             << (4 * (4 - i));
    }
    return x;
}

HandEvaluation evaluate_five_cards_fast(const std::uint8_t ranks[5], const std::uint8_t suits[5]) {
    return evaluate_five_core(ranks, suits);
}

HandEvaluation evaluate_best_hand_fast(const std::uint8_t ranks[], const std::uint8_t suits[],
                                       int card_count) {
    if (card_count <= 0) {
        return HandEvaluation{};
    }
    if (card_count < 5) {
        return partial_high_card_fast(ranks, card_count);
    }
    HandEvaluation best{};
    bool init = false;
    std::uint8_t r5[5];
    std::uint8_t s5[5];
    if (card_count == 7) {
        for (const auto& pick : kSevenChooseFive) {
            for (int i = 0; i < 5; ++i) {
                const int idx = pick[static_cast<std::size_t>(i)];
                r5[i] = ranks[idx];
                s5[i] = suits[idx];
            }
            const HandEvaluation ev = evaluate_five_core(r5, s5);
            if (!init || best < ev) {
                best = ev;
                init = true;
            }
        }
        return best;
    }
    std::array<int, 7> idx{0, 1, 2, 3, 4};
    const int n = card_count;
    auto bump = [&]() -> bool {
        int i = 4;
        while (i >= 0 && idx[static_cast<std::size_t>(i)] == n - (5 - i)) {
            --i;
        }
        if (i < 0) {
            return false;
        }
        ++idx[static_cast<std::size_t>(i)];
        for (int j = i + 1; j < 5; ++j) {
            idx[static_cast<std::size_t>(j)] = idx[static_cast<std::size_t>(j - 1)] + 1;
        }
        return true;
    };
    do {
        for (int k = 0; k < 5; ++k) {
            const int id = idx[static_cast<std::size_t>(k)];
            r5[k] = ranks[id];
            s5[k] = suits[id];
        }
        const HandEvaluation ev = evaluate_five_core(r5, s5);
        if (!init || best < ev) {
            best = ev;
            init = true;
        }
    } while (bump());
    return best;
}

std::uint64_t evaluate_seven_strength_fast(const std::uint8_t ranks[7],
                                           const std::uint8_t suits[7]) {
    return pack_hand_strength(evaluate_best_hand_fast(ranks, suits, 7));
}

std::uint64_t evaluate_hand_strength_fast(const std::vector<Card>& player_hand,
                                          const std::vector<Card>& community_cards) {
    std::uint8_t ranks[7]{};
    std::uint8_t suits[7]{};
    fill_seven_from_cards(player_hand, community_cards, ranks, suits);
    const int n = static_cast<int>(player_hand.size() + community_cards.size());
    return pack_hand_strength(evaluate_best_hand_fast(ranks, suits, n));
}

int compare_seven_strength_fast(const std::uint8_t ranks_a[7], const std::uint8_t suits_a[7],
                                const std::uint8_t ranks_b[7], const std::uint8_t suits_b[7]) {
    const std::uint64_t sa = evaluate_seven_strength_fast(ranks_a, suits_a);
    const std::uint64_t sb = evaluate_seven_strength_fast(ranks_b, suits_b);
    if (sa < sb) {
        return -1;
    }
    if (sa > sb) {
        return 1;
    }
    return 0;
}

EvaluatorBenchmarkResult benchmark_evaluator_throughput(std::size_t iterations) {
    if (iterations == 0) {
        iterations = 200000;
    }
    std::mt19937 rng(0xC47U);
    std::uniform_int_distribution<int> rank_dist(0, 12);
    std::uniform_int_distribution<int> suit_dist(0, 3);

    std::vector<Card> hole(2);
    std::vector<Card> board(5);

    const auto bench_legacy = [&]() {
        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < iterations; ++i) {
            hole[0] = Card(static_cast<std::uint8_t>(rank_dist(rng)),
                           static_cast<std::uint8_t>(suit_dist(rng)));
            hole[1] = Card(static_cast<std::uint8_t>(rank_dist(rng)),
                           static_cast<std::uint8_t>(suit_dist(rng)));
            for (int b = 0; b < 5; ++b) {
                board[static_cast<std::size_t>(b)] =
                    Card(static_cast<std::uint8_t>(rank_dist(rng)),
                         static_cast<std::uint8_t>(suit_dist(rng)));
            }
            (void)evaluate_hand_strength(hole, board);
        }
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(t1 - t0).count();
    };

    const auto bench_fast = [&]() {
        std::uint8_t ranks[7];
        std::uint8_t suits[7];
        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < iterations; ++i) {
            for (int c = 0; c < 7; ++c) {
                ranks[c] = static_cast<std::uint8_t>(rank_dist(rng));
                suits[c] = static_cast<std::uint8_t>(suit_dist(rng));
            }
            (void)evaluate_seven_strength_fast(ranks, suits);
        }
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(t1 - t0).count();
    };

    const double legacy_sec = bench_legacy();
    const double fast_sec = bench_fast();
    EvaluatorBenchmarkResult out{};
    out.legacy_evals_per_second =
        legacy_sec > 0.0 ? static_cast<double>(iterations) / legacy_sec : 0.0;
    out.fast_evals_per_second = fast_sec > 0.0 ? static_cast<double>(iterations) / fast_sec : 0.0;
    return out;
}

}  // namespace poker
