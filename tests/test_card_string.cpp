#include "poker/card_string.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

char rank_char(int r) {
    const char* ranks = "23456789TJQKA";
    return ranks[r];
}
char suit_char(int s) {
    const char* suits = "cdhs";
    return suits[s];
}

}  // namespace

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

TEST(CardString, ParseUncheckedWhitespace) {
    poker::Card c{};
    const char* p = " ah ";
    std::size_t n = 4;
    ASSERT_TRUE(poker::parse_card_string_unchecked(p, n, c));
    EXPECT_EQ(c.rank(), 12);
    EXPECT_EQ(c.suit(), 2);
}

TEST(CardString, ParseUncheckedCase) {
    poker::Card c{};
    ASSERT_TRUE(poker::parse_card_string_unchecked("AC", 2, c));
    EXPECT_EQ(c.rank(), 12);
    EXPECT_EQ(c.suit(), 0);
}

TEST(CardString, Invalid) {
    poker::Card c{};
    EXPECT_FALSE(poker::parse_card_string("", c));
    EXPECT_FALSE(poker::parse_card_string("Xx", c));
    EXPECT_FALSE(poker::parse_card_string("A", c));
}

TEST(CardString, AllCanonicalStringsRoundTrip) {
    const char* ranks = "23456789TJQKA";
    const char* suits = "cdhs";
    for (int r = 0; r < 13; ++r) {
        for (int s = 0; s < 4; ++s) {
            const std::string card = std::string{rank_char(r), suit_char(s)};
            poker::Card c{};
            ASSERT_TRUE(poker::parse_card_string(card, c)) << card;
            EXPECT_EQ(poker::deck_index_from_card(c), r * 4 + s);
        }
    }
}

TEST(CardString, DuplicateDetection) {
    EXPECT_FALSE(poker::card_strings_have_duplicate({"Ah", "Kd", "Qs"}));
    EXPECT_TRUE(poker::card_strings_have_duplicate({"Ah", "Ah"}));
    EXPECT_THROW(poker::card_strings_have_duplicate({"Ah", "bad"}), std::invalid_argument);
}

TEST(CardString, PackedDuplicateDetection) {
    const std::uint8_t ok[] = {50, 48};
    std::string err;
    EXPECT_FALSE(poker::packed_cards_have_duplicate(ok, 2, &err));
    EXPECT_TRUE(err.empty());
    const std::uint8_t dup[] = {50, 50};
    EXPECT_TRUE(poker::packed_cards_have_duplicate(dup, 2, &err));
    const std::uint8_t bad[] = {52};
    err.clear();
    EXPECT_FALSE(poker::packed_cards_have_duplicate(bad, 1, &err));
    EXPECT_FALSE(err.empty());
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

TEST(CardString, ParseCompactIndices) {
    const auto idx = poker::parse_compact_card_list_indices("AhKh");
    ASSERT_EQ(idx.size(), 2U);
    EXPECT_EQ(idx[0], 50);
    EXPECT_EQ(idx[1], 48);
}
