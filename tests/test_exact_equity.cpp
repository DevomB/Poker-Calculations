#include "poker/exact_equity.hpp"

#include "poker/card.hpp"

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
