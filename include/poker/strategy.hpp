#pragma once

#include "poker/cancel.hpp"
#include "poker/bot_config.hpp"
#include "poker/card.hpp"
#include "poker/game_state.hpp"
#include "poker/opponent_model.hpp"
#include "poker/poker_math.hpp"
#include "poker/types.hpp"

namespace poker {

/// Index of the deciding player: `hero_seat` when >= 0 (or -1 if that seat is empty), otherwise
/// the acting player, otherwise player 0.
[[nodiscard]] int hero_player_index(const PokerGameState& state, int hero_seat);

/**
 * Rule-based no-limit decision from Monte Carlo equity (`cfg.monte_carlo_simulations` >= 1 trials),
 * pot odds, and call EV. With nothing to call, equity is against `cfg.monte_carlo_villains` random
 * hands. A call is priced on side-pot layers (`side_pot_ladder_from_commitments` after hero's chips
 * go in): hero's expected winnings from each layer it can win, against random hands for that
 * layer's live players, minus the call (its stack when short). Raises are
 * sized inside the legal range. Throws std::invalid_argument when the simulation count is below 1,
 * the hero seat is empty, or hero cannot act (hand over, folded, or all-in). The optional
 * `OpponentModel` shifts equity by `(aggression_factor - 0.5) * cfg.opponent_aggression_weight`, a
 * heuristic, not a range inference.
 */
[[nodiscard]] Decision decide_action(const PokerGameState& game_state,
                                     const std::vector<Card>& player_hand,
                                     const BotConfig& cfg,
                                     const OpponentModel* opponent_model = nullptr,
                                     int hero_seat = -1,
                                     const CancelPredicate* should_cancel = nullptr);

}  // namespace poker
