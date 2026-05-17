#include "poker/exact_equity.hpp"

#include "poker/card.hpp"
#include "poker/poker_math.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

TEST(ExactEquity, RiverDominatingHand) {
    const std::vector<poker::Card> hero = {poker::Card(12, 3), poker::Card(12, 2)};  // As Ah
    const std::vector<poker::Card> board = {
        poker::Card(11, 1),
        poker::Card(9, 2),
        poker::Card(5, 0),
        poker::Card(3, 3),
        poker::Card(1, 0),
    };
    const double eq = poker::exact_hu_equity_vs_random_hand(hero, board);
    EXPECT_GT(eq, 0.85);
    EXPECT_LE(eq, 1.0);
}

TEST(ExactEquity, FlopEnumerationRuns) {
    const std::vector<poker::Card> hero = {poker::Card(12, 3), poker::Card(11, 3)};  // As Ks
    const std::vector<poker::Card> board = {
        poker::Card(10, 3),
        poker::Card(9, 3),
        poker::Card(2, 0),
    };
    const double eq = poker::exact_hu_equity_vs_random_hand(hero, board);
    EXPECT_GT(eq, 0.0);
    EXPECT_LT(eq, 1.0);
}

TEST(ExactEquity, ChubukovBinarySearchMatchesClosedForm) {
    const std::vector<poker::Card> hero = {poker::Card(12, 3), poker::Card(11, 3)};
    const std::vector<poker::Card> board = {
        poker::Card(10, 3),
        poker::Card(9, 3),
        poker::Card(2, 0),
    };
    const double eq = poker::exact_hu_equity_vs_random_hand(hero, board);
    const double dead = 150.0;
    const double closed = poker::chubukov_symmetric_jam_breakeven_stack(dead, eq);
    const double searched =
        poker::chubukov_max_symmetric_jam_stack_binary_search(hero, board, dead, 1e9, 80);
    if (std::isfinite(closed)) {
        EXPECT_NEAR(searched, std::min(1e9, closed), 1e-6 * std::max(1.0, closed));
    }
}

TEST(ExactEquity, ChubukovFromHandBinarySearchMatchesEquitySearch) {
    const std::vector<poker::Card> hero = {poker::Card(12, 3), poker::Card(11, 3)};
    const std::vector<poker::Card> board = {
        poker::Card(10, 3),
        poker::Card(9, 3),
        poker::Card(2, 0),
    };
    const double eq = poker::exact_hu_equity_vs_random_hand(hero, board);
    const double dead = 150.0;
    const int cap = 1'000'000'000;
    const int from_hand =
        poker::chubukov_max_symmetric_jam_stack_from_hand_binary_search(hero, board, dead, cap);
    const int from_eq = poker::chubukov_max_symmetric_jam_stack_chips_binary_search(eq, dead, cap);
    EXPECT_EQ(from_hand, from_eq);
}

TEST(ExactEquity, StraightMadeFlopToRiverAlreadyStraightOnFlop) {
    const std::vector<poker::Card> hero = {poker::Card(3, 0), poker::Card(4, 0)};  // 5c 6c
    const std::vector<poker::Card> flop = {
        poker::Card(5, 1),
        poker::Card(6, 1),
        poker::Card(7, 2),
    };  // 7d 8d 9h
    const std::vector<poker::Card> dead;
    const double p = poker::straight_made_flop_to_river_exact_probability(hero, flop, dead);
    EXPECT_NEAR(p, 1.0, 1e-12);
}

TEST(ExactEquity, StraightMadeFlopToRiverOpenEndedStrictlyBetweenZeroAndOne) {
    const std::vector<poker::Card> hero = {poker::Card(6, 0), poker::Card(7, 0)};  // 8h 9h
    const std::vector<poker::Card> flop = {
        poker::Card(8, 1),
        poker::Card(9, 2),
        poker::Card(0, 3),
    };  // Td Jc 2s
    const double p = poker::straight_made_flop_to_river_exact_probability(hero, flop, {});
    EXPECT_GT(p, 0.0);
    EXPECT_LT(p, 1.0);
}
