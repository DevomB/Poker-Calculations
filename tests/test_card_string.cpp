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

TEST(CardString, CanonicalTen) {
    EXPECT_EQ(poker::canonical_card_string("10h"), "Th");
}

TEST(CardString, ParseCompact) {
    const auto a = poker::parse_compact_card_list("AhKh");
    ASSERT_EQ(a.size(), 2U);
    EXPECT_EQ(a[0], "Ah");
    EXPECT_EQ(a[1], "Kh");
    const auto b = poker::parse_compact_card_list(" 10h Kd ");
    ASSERT_EQ(b.size(), 2U);
    EXPECT_EQ(b[0], "Th");
    EXPECT_EQ(b[1], "Kd");
    EXPECT_THROW(poker::parse_compact_card_list("AhAh"), std::invalid_argument);
}
