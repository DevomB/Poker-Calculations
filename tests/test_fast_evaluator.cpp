#include <gtest/gtest.h>

#include "poker/card_string.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/hand_evaluator.hpp"

#include <random>
#include <vector>

namespace {

std::vector<poker::Card> parse_list(const std::vector<std::string>& cards) {
    std::vector<poker::Card> out;
    out.reserve(cards.size());
    for (const auto& s : cards) {
        poker::Card c;
        ASSERT_TRUE(poker::parse_card_string(s, c));
        out.push_back(c);
    }
    return out;
}

}  // namespace

TEST(FastEvaluator, MatchesLegacyOnKnownHands) {
    const auto hole = parse_list({"As", "Ah"});
    const auto board = parse_list({"Kd", "Ks", "Qh", "Jc", "2d"});
    const std::uint64_t legacy = poker::evaluate_hand_strength(hole, board);
    const std::uint64_t fast = poker::evaluate_hand_strength_fast(hole, board);
    EXPECT_EQ(legacy, fast);
}

TEST(FastEvaluator, RandomParityWithLegacy) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> rank_dist(0, 12);
    std::uniform_int_distribution<int> suit_dist(0, 3);
    std::vector<poker::Card> hole(2);
    std::vector<poker::Card> board(5);
    for (int trial = 0; trial < 5000; ++trial) {
        hole[0] = poker::Card(static_cast<std::uint8_t>(rank_dist(rng)),
                              static_cast<std::uint8_t>(suit_dist(rng)));
        hole[1] = poker::Card(static_cast<std::uint8_t>(rank_dist(rng)),
                              static_cast<std::uint8_t>(suit_dist(rng)));
        for (int b = 0; b < 5; ++b) {
            board[static_cast<std::size_t>(b)] =
                poker::Card(static_cast<std::uint8_t>(rank_dist(rng)),
                            static_cast<std::uint8_t>(suit_dist(rng)));
        }
        EXPECT_EQ(poker::evaluate_hand_strength(hole, board),
                  poker::evaluate_hand_strength_fast(hole, board));
    }
}

TEST(FastEvaluator, BenchmarkReportsPositiveThroughput) {
    const auto bench = poker::benchmark_evaluator_throughput(50000);
    EXPECT_GT(bench.legacy_evals_per_second, 0.0);
    EXPECT_GT(bench.fast_evals_per_second, 0.0);
    EXPECT_STREQ(bench.implementation, "poker-calculations-forge");
}
