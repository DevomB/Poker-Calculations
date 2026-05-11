#pragma once

#include "poker/bot_config.hpp"
#include "poker/card.hpp"
#include "poker/game_state.hpp"
#include "poker/opponent_model.hpp"
#include "poker/poker_math.hpp"
#include "poker/types.hpp"

namespace poker {

/// Uses pot odds, optional Monte Carlo equity (`cfg.monte_carlo_simulations`), and `OpponentModel`.
[[nodiscard]] Decision decide_action(const PokerGameState& game_state,
                                     const std::vector<Card>& player_hand,
                                     const BotConfig& cfg,
                                     const OpponentModel* opponent_model = nullptr,
                                     int hero_seat = -1);

}  // namespace poker
