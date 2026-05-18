#include "poker/card_string.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(CardString, ParseTen) {
    poker::Card c{};
    ASSERT_TRUE(poker::parse_card_string("  10h ", c));
    EXPECT_EQ(c.rank(), 8);
    EXPECT_EQ(c.suit(), 2);
}

TEST(CardString, ParseAce) {
    poker::Card c{};
    ASSERT_TRUE(poker::parse_card_string("Ac", c));
    EXPECT_EQ(c.rank(), 12);
    EXPECT_EQ(c.suit(), 0);
}

TEST(CardString, Invalid) {
    poker::Card c{};
    EXPECT_FALSE(poker::parse_card_string("", c));
    EXPECT_FALSE(poker::parse_card_string("Xx", c));
    EXPECT_FALSE(poker::parse_card_string("A", c));
}

TEST(CardString, DuplicateDetection) {
    EXPECT_FALSE(poker::card_strings_have_duplicate({"Ah", "Kd", "Qs"}));
    EXPECT_TRUE(poker::card_strings_have_duplicate({"Ah", "Ah"}));
    EXPECT_THROW(poker::card_strings_have_duplicate({"Ah", "bad"}), std::invalid_argument);
}
