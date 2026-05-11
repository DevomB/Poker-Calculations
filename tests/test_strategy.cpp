#include "poker/strategy.hpp"

#include "poker/game_engine.hpp"

#include <gtest/gtest.h>

TEST(Strategy, DecideFoldsWeakFacingBet) {
    poker::PokerGameState state;
    state.players.push_back({"H", {}, 100, 0, 0, false, 0});
    state.players.push_back({"V", {}, 100, 0, 0, false, 1});
    state.phase = poker::GamePhase::Flop;
    state.pot = 40;
    state.big_blind = 2;
    state.community_cards = {
        poker::Card{10, 0}, poker::Card{9, 1}, poker::Card{8, 2},
    };
    state.players[0].hole_cards = {poker::Card{2, 0}, poker::Card{3, 1}};
    state.players[0].committed_this_street = 0;
    state.players[1].committed_this_street = 20;
    state.current_bet = 20;
    state.acted_this_street.assign(state.players.size(), false);
    state.acting_index = 0;

    poker::BotConfig cfg;
    cfg.monte_carlo_simulations = 200;
    cfg.rng_seed = 7;
    cfg.risk_tolerance = 0.5F;

    const auto d = poker::decide_action(state, state.players[0].hole_cards, cfg, nullptr, 0);
    EXPECT_EQ(d.action, poker::Action::Fold);
}
