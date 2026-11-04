#pragma once

#include "poker/deck.hpp"
#include "poker/game_state.hpp"
#include "poker/types.hpp"

#include <random>

namespace poker {

struct GameEngineConfig {
    int small_blind{1};
    int big_blind{2};
    int button_seat{0};
};

/**
 * Mutable no-limit engine: owns RNG and deck for dealing and mutates `PokerGameState`.
 *
 * Rules: heads-up the button posts the small blind and acts first before the flop; a short
 * (all-in) blind still leaves the full big blind to call; an all-in raise short of a full raise
 * keeps the previous minimum raise and does not reopen betting for players who already acted
 * (`legal_actions_for`); when at most one player can still bet the board runs out to `Showdown`;
 * when one player is left the hand ends in `HandComplete`. The engine does not award the pot: at
 * `HandComplete` it belongs to the last player in, and at `Showdown` the side-pot helpers split it
 * by `total_committed_hand`. Chips are conserved: `pot + sum(stacks)` never changes.
 */
class GameEngine {
public:
    explicit GameEngine(std::uint32_t seed = std::random_device{}());

    void start_new_hand(PokerGameState& state, const GameEngineConfig& cfg);
    void advance_phase_if_ready(PokerGameState& state);

    /// Applies a decision for `state.acting_index`. Returns false (state unchanged) if illegal.
    /// A raise below the minimum is raised to it and one above the stack is capped at all-in.
    bool apply_action(PokerGameState& state, Decision d);

    [[nodiscard]] bool betting_round_complete(const PokerGameState& state) const;

    std::mt19937& rng() { return rng_; }

private:
    void deal_hole_cards(PokerGameState& state);
    void post_blinds(PokerGameState& state);
    void reset_street_bets(PokerGameState& state);
    void deal_board_for_phase(PokerGameState& state);
    void set_next_actor(PokerGameState& state);
    [[nodiscard]] int next_seat_after(int start, const PokerGameState& state) const;

    Deck deck_;
    std::mt19937 rng_;
};

}  // namespace poker
