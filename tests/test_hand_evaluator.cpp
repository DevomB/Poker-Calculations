#include "poker/hand_evaluator.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

static poker::Card C(std::uint8_t r, std::uint8_t s) { return poker::Card{r, s}; }

TEST(HandEvaluator, RoyalFlushBeatsStraightFlush) {
    std::vector<poker::Card> royal = {C(12, 3), C(11, 3), C(10, 3), C(9, 3), C(8, 3)};
    std::vector<poker::Card> sf = {C(7, 0), C(6, 0), C(5, 0), C(4, 0), C(3, 0)};
    auto a = poker::evaluate_five_cards(royal);
    auto b = poker::evaluate_five_cards(sf);
    EXPECT_EQ(a.rank, poker::HandRank::RoyalFlush);
    EXPECT_EQ(b.rank, poker::HandRank::StraightFlush);
    EXPECT_LT(b, a);
}

TEST(HandEvaluator, WheelStraight) {
    std::vector<poker::Card> wheel = {C(12, 0), C(0, 1), C(1, 2), C(2, 3), C(3, 0)};
    auto e = poker::evaluate_five_cards(wheel);
    EXPECT_EQ(e.rank, poker::HandRank::Straight);
    EXPECT_EQ(e.kickers[0], 3);
}

TEST(HandEvaluator, BestSevenChoosesNutFlush) {
    std::vector<poker::Card> seven = {
        C(12, 0), C(10, 0), C(2, 1), C(5, 0), C(7, 0), C(3, 0), C(9, 2),
    };
    auto e = poker::evaluate_best_hand(seven);
    EXPECT_EQ(e.rank, poker::HandRank::Flush);
}

TEST(HandEvaluator, PairBeatsHighCard) {
    std::vector<poker::Card> pair = {C(12, 0), C(12, 1), C(5, 2), C(3, 3), C(9, 0)};
    std::vector<poker::Card> high = {C(11, 0), C(9, 1), C(7, 2), C(5, 3), C(3, 0)};
    EXPECT_LT(poker::evaluate_five_cards(high), poker::evaluate_five_cards(pair));
}

TEST(HandEvaluator, CompareBestHandsOrder) {
    std::vector<poker::Card> aa = {C(12, 0), C(12, 1)};
    std::vector<poker::Card> kk = {C(11, 0), C(11, 1)};
    EXPECT_EQ(poker::compare_best_hands(aa, kk), 1);
    EXPECT_EQ(poker::compare_best_hands(kk, aa), -1);
    std::vector<poker::Card> aa2 = {C(12, 2), C(12, 3)};
    EXPECT_EQ(poker::compare_best_hands(aa, aa2), 0);
}

TEST(HandEvaluator, CompareBestHandsOverlapThrows) {
    std::vector<poker::Card> a = {C(12, 0), C(12, 1)};
    std::vector<poker::Card> b = {C(11, 0), C(12, 0)};
    EXPECT_THROW(poker::compare_best_hands(a, b), std::invalid_argument);
}
