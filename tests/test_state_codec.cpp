#include "poker/state_codec.hpp"

#include "poker/card.hpp"

#include <gtest/gtest.h>

#include <cstring>

TEST(StateCodec, RoundTripMinimal) {
    poker::PokerGameState state{};
    state.phase = poker::GamePhase::Flop;
    state.pot = 40;
    state.current_bet = 0;
    state.button_seat = 0;
    state.small_blind = 1;
    state.big_blind = 2;
    state.acting_index = 0;
    state.acted_this_street = {false, false};

    poker::Player p0{};
    p0.name = "Hero";
    p0.hole_cards = {poker::Card{12, 0}, poker::Card{11, 1}};
    p0.stack = 200;
    p0.seat = 0;
    poker::Player p1{};
    p1.name = "Villain";
    p1.hole_cards = {poker::Card{0, 2}, poker::Card{1, 3}};
    p1.stack = 180;
    p1.seat = 1;
    state.players = {p0, p1};
    state.community_cards = {poker::Card{9, 0}, poker::Card{8, 1}, poker::Card{7, 2}};

    const auto bytes = poker::encode_poker_state(state);
    ASSERT_FALSE(bytes.empty());
    EXPECT_EQ(std::memcmp(bytes.data(), poker::kStateCodecMagic, 4), 0);

    poker::PokerGameState decoded{};
    std::string err;
    ASSERT_TRUE(poker::decode_poker_state(bytes.data(), bytes.size(), decoded, &err)) << err;
    EXPECT_EQ(decoded.pot, state.pot);
    EXPECT_EQ(decoded.players.size(), 2u);
    EXPECT_EQ(decoded.community_cards.size(), 3u);
    EXPECT_EQ(decoded.phase, state.phase);
}

TEST(StateCodec, BadMagicRejected) {
    std::vector<std::uint8_t> bad = {0, 0, 0, 0, poker::kStateCodecVersion, 0};
    poker::PokerGameState out{};
    std::string err;
    EXPECT_FALSE(poker::decode_poker_state(bad.data(), bad.size(), out, &err));
    EXPECT_FALSE(err.empty());
}
