#include "poker/card_string.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace poker {
namespace {

TEST(PackedCards, DeckIndexRoundTripAll52) {
    for (int idx = 0; idx < 52; ++idx) {
        const Card c = card_from_deck_index(idx);
        EXPECT_EQ(deck_index_from_card(c), idx);
    }
}

TEST(PackedCards, CardFromDeckIndexInvalid) {
    EXPECT_THROW(card_from_deck_index(-1), std::invalid_argument);
    EXPECT_THROW(card_from_deck_index(52), std::invalid_argument);
}

TEST(PackedCards, ParsePackedCardsEmpty) {
    std::vector<Card> out;
    EXPECT_TRUE(parse_packed_cards(nullptr, 0, out, nullptr));
    EXPECT_TRUE(out.empty());
}

TEST(PackedCards, ParsePackedCardsInvalidByte) {
    const std::uint8_t data[] = {0, 52};
    std::vector<Card> out;
    std::string err;
    EXPECT_FALSE(parse_packed_cards(data, 2, out, &err));
    EXPECT_NE(err.find("index 1"), std::string::npos);
}

TEST(PackedCards, ParsePackedCardsValid) {
    const std::uint8_t data[] = {0, 51};
    std::vector<Card> out;
    EXPECT_TRUE(parse_packed_cards(data, 2, out, nullptr));
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(deck_index_from_card(out[0]), 0);
    EXPECT_EQ(deck_index_from_card(out[1]), 51);
}

}  // namespace
}  // namespace poker
