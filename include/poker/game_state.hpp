#pragma once

#include "poker/card.hpp"
#include "poker/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace poker {

/// Every chip field, and the table total `pot + sum(stacks)`, must fit in a signed 32-bit integer
/// (the PKST field width), so engine arithmetic on a valid state cannot overflow.
inline constexpr std::int64_t kMaxStateChips = 2147483647;
/// Seats are stored in one PKST byte.
inline constexpr int kMaxStateSeat = 255;
inline constexpr std::size_t kMinStatePlayers = 2;
inline constexpr std::size_t kMaxStatePlayers = 10;

struct Player {
    std::string name;
    std::vector<Card> hole_cards;
    int stack{0};
    int committed_this_street{0};
    int total_committed_hand{0};
    bool folded{false};
    int seat{0};
};

struct PokerGameState {
    std::vector<Player> players;
    std::vector<Card> community_cards;
    GamePhase phase{GamePhase::PreFlop};
    int pot{0};
    /// Maximum committed_this_street among active players this betting round.
    int current_bet{0};
    int button_seat{0};
    int small_blind{1};
    int big_blind{2};
    /// Seat index (into `players`) whose action is pending; -1 if none.
    int acting_index{-1};
    int last_raise_increment{0};
    /// First to act this betting street (seat order handled by engine).
    int street_opening_index{-1};
    /// Voluntary actions this street; reset on each street and on any raise.
    std::vector<bool> acted_this_street;

    [[nodiscard]] int to_call(const Player& p) const;
    [[nodiscard]] int active_players_in_hand() const;
};

/// Snapshot for strategy layer (hero-centric).
[[nodiscard]] BotView make_bot_view(const PokerGameState& state, int hero_seat);

/**
 * Consistency problems in a state snapshot; empty when the snapshot is valid. Checks player count
 * (2-10), non-negative chip fields and the 32-bit table total, unique seats (0-255) and cards,
 * hole cards (2 known cards, or none for an unknown hand), board length against the phase,
 * `pot >= sum(committedThisStreet)`, `currentBet >= max(committedThisStreet)`, one
 * `actedThisStreet` flag per player, and actor indices. A finished hand is a valid snapshot;
 * whether an operation can act on it is decided by that operation (see `legal_actions_for`).
 */
[[nodiscard]] std::vector<std::string> poker_state_errors(const PokerGameState& state);

/// Index into `state.players` of the player in `seat`, or -1.
[[nodiscard]] int player_index_for_seat(const PokerGameState& state, int seat);

/// Why `player_index` cannot act in `state`, or nullptr when it can (the hand is over, the player
/// is missing, has folded, or is all-in).
[[nodiscard]] const char* unavailable_actor_reason(const PokerGameState& state, int player_index);

struct LegalActions {
    bool can_fold{false};
    bool can_check{false};
    bool can_call{false};
    bool can_raise{false};
    /// Chips needed to match the current bet; may exceed the stack (calling then puts hero all-in).
    int to_call{0};
    /// Smallest legal raise-to total this street: the current bet plus the last full raise, or
    /// all-in when the stack is shorter than that.
    int min_raise_to{0};
    /// All-in raise-to total (`committedThisStreet + stack`).
    int max_raise_to{0};
};

/**
 * No-limit legal actions for `player_index`. A player who already acted this street and now faces
 * less than a full raise (an all-in short raise) may call or fold but not re-raise. Everything is
 * false when `unavailable_actor_reason` is non-null.
 */
[[nodiscard]] LegalActions legal_actions_for(const PokerGameState& state, int player_index);

}  // namespace poker
