#include "poker/game_engine.hpp"

#include "poker/card_string.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace poker {

namespace {

int max_committed_among_active(const PokerGameState& state) {
    int m = 0;
    for (const auto& pl : state.players) {
        if (!pl.folded) {
            m = std::max(m, pl.committed_this_street);
        }
    }
    return m;
}

bool player_can_act(const Player& p) { return !p.folded && p.stack > 0; }

bool hand_over(GamePhase phase) {
    return phase == GamePhase::Showdown || phase == GamePhase::HandComplete;
}

/// The bet everyone still in must match. `current_bet` can exceed every commitment when the big
/// blind posted short: callers still owe the full big blind.
int bet_to_match(const PokerGameState& state) {
    return std::max(state.current_bet, max_committed_among_active(state));
}

/// Size of the last full bet or raise this street; never below the big blind or one chip.
int full_raise_size(const PokerGameState& state) {
    return std::max({state.last_raise_increment, state.big_blind, 1});
}

bool has_acted(const PokerGameState& state, std::size_t i) {
    return i < state.acted_this_street.size() && state.acted_this_street[i];
}

void put_in(PokerGameState& state, Player& p, int amount) {
    p.stack -= amount;
    p.committed_this_street += amount;
    p.total_committed_hand += amount;
    state.pot += amount;
}

/// First player from `start` (wrapping) who still owes chips or has not acted; -1 if none.
int next_actor_from(const PokerGameState& state, int start) {
    const int n = static_cast<int>(state.players.size());
    if (n == 0) {
        return -1;
    }
    start = (start < 0 || start >= n) ? 0 : start;
    const int target = bet_to_match(state);
    for (int k = 0; k < n; ++k) {
        const int idx = (start + k) % n;
        const Player& p = state.players[static_cast<std::size_t>(idx)];
        if (player_can_act(p) &&
            (p.committed_this_street < target || !has_acted(state, static_cast<std::size_t>(idx)))) {
            return idx;
        }
    }
    return -1;
}

std::string player_label(std::size_t i) { return "players[" + std::to_string(i) + "]"; }

const char* phase_label(GamePhase phase) {
    switch (phase) {
        case GamePhase::PreFlop:
            return "PreFlop";
        case GamePhase::Flop:
            return "Flop";
        case GamePhase::Turn:
            return "Turn";
        case GamePhase::River:
            return "River";
        case GamePhase::Showdown:
            return "Showdown";
        case GamePhase::HandComplete:
            return "HandComplete";
    }
    return "PreFlop";
}

}  // namespace

int PokerGameState::to_call(const Player& p) const {
    if (p.folded) {
        return 0;
    }
    return std::max(0, current_bet - p.committed_this_street);
}

int PokerGameState::active_players_in_hand() const {
    int n = 0;
    for (const auto& pl : players) {
        if (!pl.folded) {
            ++n;
        }
    }
    return n;
}

BotView make_bot_view(const PokerGameState& state, int hero_seat) {
    BotView v;
    v.hero_seat = hero_seat;
    v.pot = state.pot;
    v.big_blind = state.big_blind;
    const Player* hero = nullptr;
    for (const auto& p : state.players) {
        if (p.seat == hero_seat) {
            hero = &p;
            break;
        }
    }
    if (!hero) {
        return v;
    }
    v.hero_stack = hero->stack;
    v.to_call = state.to_call(*hero);
    v.can_check = v.to_call == 0;
    return v;
}

int player_index_for_seat(const PokerGameState& state, int seat) {
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        if (state.players[i].seat == seat) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const char* unavailable_actor_reason(const PokerGameState& state, int player_index) {
    if (hand_over(state.phase)) {
        return "the hand is over";
    }
    if (player_index < 0 || player_index >= static_cast<int>(state.players.size())) {
        return "the player is not at the table";
    }
    const Player& p = state.players[static_cast<std::size_t>(player_index)];
    if (p.folded) {
        return "the player has folded";
    }
    if (p.stack <= 0) {
        return "the player is all-in";
    }
    return nullptr;
}

LegalActions legal_actions_for(const PokerGameState& state, int player_index) {
    LegalActions out{};
    const int full = full_raise_size(state);
    out.min_raise_to = state.current_bet + full;
    if (player_index >= 0 && player_index < static_cast<int>(state.players.size())) {
        const Player& p = state.players[static_cast<std::size_t>(player_index)];
        out.to_call = state.to_call(p);
        out.max_raise_to = p.committed_this_street + p.stack;
    }
    if (unavailable_actor_reason(state, player_index) != nullptr) {
        return out;
    }
    const Player& p = state.players[static_cast<std::size_t>(player_index)];
    out.can_fold = out.to_call > 0;
    out.can_check = out.to_call == 0;
    out.can_call = out.to_call > 0;
    // A short all-in raise does not reopen betting for a player who already acted: the bet must
    // have grown by at least one full raise since that player's last action.
    const bool reopened =
        !has_acted(state, static_cast<std::size_t>(player_index)) || out.to_call >= full;
    // A raise needs someone to answer it: another player still in the hand with chips behind.
    // Against opponents who are all in, the only choices are call (or check) and fold.
    bool opponent_can_respond = false;
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        if (static_cast<int>(i) != player_index && player_can_act(state.players[i])) {
            opponent_can_respond = true;
            break;
        }
    }
    out.can_raise = p.stack > out.to_call && reopened && opponent_can_respond;
    if (out.can_raise) {
        out.min_raise_to = std::min(out.min_raise_to, out.max_raise_to);
    }
    return out;
}

std::vector<std::string> poker_state_errors(const PokerGameState& state) {
    std::vector<std::string> errors;
    const std::size_t n = state.players.size();
    if (n < kMinStatePlayers || n > kMaxStatePlayers) {
        errors.push_back("players must have 2 to 10 entries (got " + std::to_string(n) + ")");
    }
    const auto non_negative = [&errors](const std::string& name, int v) {
        if (v < 0) {
            errors.push_back(name + " must be non-negative (got " + std::to_string(v) + ")");
        }
    };
    non_negative("pot", state.pot);
    non_negative("currentBet", state.current_bet);
    non_negative("smallBlind", state.small_blind);
    non_negative("bigBlind", state.big_blind);
    non_negative("lastRaiseIncrement", state.last_raise_increment);
    if (state.small_blind > state.big_blind) {
        errors.emplace_back("smallBlind must not exceed bigBlind");
    }
    if (state.button_seat < 0 || state.button_seat > kMaxStateSeat) {
        errors.emplace_back("buttonSeat must be between 0 and 255");
    }

    bool seen_card[52]{};
    const auto add_card = [&](const Card& c, const std::string& where) {
        const int id = deck_index_from_card(c);
        if (seen_card[id]) {
            errors.push_back("card " + c.to_string() + " appears twice (" + where + ")");
        }
        seen_card[id] = true;
    };

    const std::size_t board = state.community_cards.size();
    std::size_t expected_board = 0;
    bool board_fixed = true;
    switch (state.phase) {
        case GamePhase::PreFlop:
            expected_board = 0;
            break;
        case GamePhase::Flop:
            expected_board = 3;
            break;
        case GamePhase::Turn:
            expected_board = 4;
            break;
        case GamePhase::River:
        case GamePhase::Showdown:
            expected_board = 5;
            break;
        case GamePhase::HandComplete:
            board_fixed = false;
            break;
    }
    if (board_fixed && board != expected_board) {
        errors.push_back(std::string("phase ") + phase_label(state.phase) + " needs " +
                         std::to_string(expected_board) + " community cards (got " +
                         std::to_string(board) + ")");
    } else if (!board_fixed && board != 0 && (board < 3 || board > 5)) {
        errors.push_back("communityCards must hold 0, 3, 4, or 5 cards (got " + std::to_string(board) +
                         ")");
    }
    for (const Card& c : state.community_cards) {
        add_card(c, "communityCards");
    }

    bool seen_seat[kMaxStateSeat + 1]{};
    std::int64_t table_total = state.pot;
    std::int64_t street_total = 0;
    int max_street = 0;
    std::size_t still_in = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const Player& p = state.players[i];
        const std::string label = player_label(i);
        non_negative(label + ".stack", p.stack);
        non_negative(label + ".committedThisStreet", p.committed_this_street);
        non_negative(label + ".totalCommittedHand", p.total_committed_hand);
        if (p.seat < 0 || p.seat > kMaxStateSeat) {
            errors.push_back(label + ".seat must be between 0 and 255");
        } else if (seen_seat[p.seat]) {
            errors.push_back(label + ".seat " + std::to_string(p.seat) + " is used twice");
        } else {
            seen_seat[p.seat] = true;
        }
        if (!p.hole_cards.empty() && p.hole_cards.size() != 2) {
            errors.push_back(label + ".holeCards must hold 2 cards, or none for an unknown hand (got " +
                             std::to_string(p.hole_cards.size()) + ")");
        }
        for (const Card& c : p.hole_cards) {
            add_card(c, label + ".holeCards");
        }
        table_total += p.stack;
        street_total += p.committed_this_street;
        max_street = std::max(max_street, p.committed_this_street);
        still_in += p.folded ? 0 : 1;
    }
    if (n > 0 && still_in == 0) {
        errors.emplace_back("every player has folded");
    }
    if (table_total > kMaxStateChips) {
        errors.emplace_back("pot plus all stacks must not exceed 2147483647 chips");
    }
    if (street_total > state.pot) {
        errors.emplace_back("pot must include every player's committedThisStreet");
    }
    if (max_street > state.current_bet) {
        errors.emplace_back("currentBet must be at least the largest committedThisStreet");
    }
    if (state.acted_this_street.size() != n) {
        errors.push_back("actedThisStreet must have one flag per player (got " +
                         std::to_string(state.acted_this_street.size()) + " for " + std::to_string(n) +
                         " players)");
    }
    if (state.acting_index < -1 || state.acting_index >= static_cast<int>(n)) {
        errors.emplace_back("actingIndex must be -1 or an index into players");
    } else if (state.acting_index >= 0) {
        const Player& actor = state.players[static_cast<std::size_t>(state.acting_index)];
        if (hand_over(state.phase)) {
            errors.emplace_back("actingIndex must be -1 once the hand is over");
        } else if (actor.folded || actor.stack <= 0) {
            errors.emplace_back("actingIndex points at a player who cannot act (folded or all-in)");
        }
    }
    if (state.street_opening_index < -1 || state.street_opening_index >= static_cast<int>(n)) {
        errors.emplace_back("streetOpeningIndex must be -1 or an index into players");
    }
    return errors;
}

GameEngine::GameEngine(std::uint32_t seed) : rng_(seed) {}

void GameEngine::start_new_hand(PokerGameState& state, const GameEngineConfig& cfg) {
    std::sort(state.players.begin(), state.players.end(),
              [](const Player& a, const Player& b) { return a.seat < b.seat; });
    state.community_cards.clear();
    state.pot = 0;
    state.current_bet = 0;
    state.last_raise_increment = 0;
    state.acted_this_street.assign(state.players.size(), false);
    state.phase = GamePhase::PreFlop;
    state.small_blind = cfg.small_blind;
    state.big_blind = cfg.big_blind;
    state.button_seat = cfg.button_seat;
    state.acting_index = -1;
    state.street_opening_index = -1;

    int funded = 0;
    for (auto& p : state.players) {
        p.hole_cards.clear();
        p.committed_this_street = 0;
        p.total_committed_hand = 0;
        p.folded = p.stack <= 0;
        funded += p.folded ? 0 : 1;
    }
    if (funded < 2) {
        state.phase = GamePhase::HandComplete;
        return;
    }

    deck_.reset_deck();
    deck_.shuffle(rng_);
    deal_hole_cards(state);
    post_blinds(state);
    advance_phase_if_ready(state);
}

void GameEngine::deal_hole_cards(PokerGameState& state) {
    for (int round = 0; round < 2; ++round) {
        for (auto& p : state.players) {
            if (p.stack > 0 && !p.folded) {
                p.hole_cards.push_back(deck_.deal_card());
            }
        }
    }
}

int GameEngine::next_seat_after(int start, const PokerGameState& state) const {
    if (state.players.empty()) {
        return -1;
    }
    int best = -1;
    int best_seat = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        const auto& p = state.players[i];
        if (p.stack <= 0) {
            continue;
        }
        if (p.seat > start && p.seat < best_seat) {
            best_seat = p.seat;
            best = static_cast<int>(i);
        }
    }
    if (best >= 0) {
        return best;
    }
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        const auto& p = state.players[i];
        if (p.stack <= 0) {
            continue;
        }
        if (p.seat < best_seat) {
            best_seat = p.seat;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void GameEngine::post_blinds(PokerGameState& state) {
    int funded = 0;
    for (const auto& p : state.players) {
        funded += player_can_act(p) ? 1 : 0;
    }
    // Heads-up the button posts the small blind and acts first before the flop.
    int sb_idx = -1;
    if (funded == 2) {
        const int btn_idx = player_index_for_seat(state, state.button_seat);
        if (btn_idx >= 0 && player_can_act(state.players[static_cast<std::size_t>(btn_idx)])) {
            sb_idx = btn_idx;
        }
    }
    if (sb_idx < 0) {
        sb_idx = next_seat_after(state.button_seat, state);
    }
    const int bb_idx = next_seat_after(state.players[static_cast<std::size_t>(sb_idx)].seat, state);
    const int bb_seat = state.players[static_cast<std::size_t>(bb_idx)].seat;

    const auto post = [&](int idx, int amount) {
        Player& p = state.players[static_cast<std::size_t>(idx)];
        put_in(state, p, std::min(amount, p.stack));
    };
    post(sb_idx, state.small_blind);
    post(bb_idx, state.big_blind);

    // A short (all-in) blind does not lower the bet: callers owe the full big blind.
    state.current_bet = std::max(state.big_blind, max_committed_among_active(state));
    state.last_raise_increment = state.big_blind;
    state.acted_this_street.assign(state.players.size(), false);
    state.street_opening_index = next_seat_after(bb_seat, state);
}

void GameEngine::reset_street_bets(PokerGameState& state) {
    for (auto& p : state.players) {
        p.committed_this_street = 0;
    }
    state.current_bet = 0;
    state.last_raise_increment = state.big_blind;
    state.acted_this_street.assign(state.players.size(), false);
    // First post-flop actor: next seat after the button (the big blind heads-up).
    state.street_opening_index = next_seat_after(state.button_seat, state);
}

void GameEngine::deal_board_for_phase(PokerGameState& state) {
    if (state.phase == GamePhase::Flop) {
        if (!deck_.empty()) {
            deck_.deal_card();
        }  // burn
        for (int i = 0; i < 3 && !deck_.empty(); ++i) {
            state.community_cards.push_back(deck_.deal_card());
        }
    } else if (state.phase == GamePhase::Turn || state.phase == GamePhase::River) {
        if (!deck_.empty()) {
            deck_.deal_card();
        }
        if (!deck_.empty()) {
            state.community_cards.push_back(deck_.deal_card());
        }
    }
}

void GameEngine::set_next_actor(PokerGameState& state) {
    state.acting_index = next_actor_from(state, state.street_opening_index);
}

bool GameEngine::betting_round_complete(const PokerGameState& state) const {
    if (state.active_players_in_hand() <= 1) {
        return true;
    }
    int able = 0;
    int able_idx = -1;
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        if (player_can_act(state.players[i])) {
            ++able;
            able_idx = static_cast<int>(i);
        }
    }
    if (able == 0) {
        return true;
    }
    if (able == 1) {
        // Everyone else still in is all-in: once the last player with chips has matched them there
        // is nobody left to bet against.
        const Player& last = state.players[static_cast<std::size_t>(able_idx)];
        int others = 0;
        for (std::size_t i = 0; i < state.players.size(); ++i) {
            if (static_cast<int>(i) != able_idx && !state.players[i].folded) {
                others = std::max(others, state.players[i].committed_this_street);
            }
        }
        if (last.committed_this_street >= others) {
            return true;
        }
    }
    return next_actor_from(state, 0) < 0;
}

void GameEngine::advance_phase_if_ready(PokerGameState& state) {
    while (!hand_over(state.phase) && betting_round_complete(state)) {
        if (state.active_players_in_hand() <= 1) {
            // Everyone else folded. The pot is left for the caller to award to the last player.
            state.phase = GamePhase::HandComplete;
            state.acting_index = -1;
            return;
        }
        if (state.phase == GamePhase::River) {
            state.phase = GamePhase::Showdown;
            state.acting_index = -1;
            return;
        }
        reset_street_bets(state);
        state.phase = state.phase == GamePhase::PreFlop ? GamePhase::Flop
                      : state.phase == GamePhase::Flop  ? GamePhase::Turn
                                                        : GamePhase::River;
        deal_board_for_phase(state);
    }
    if (!hand_over(state.phase)) {
        set_next_actor(state);
    }
}

bool GameEngine::apply_action(PokerGameState& state, Decision d) {
    const int ai = state.acting_index;
    if (unavailable_actor_reason(state, ai) != nullptr) {
        return false;
    }
    if (state.acted_this_street.size() != state.players.size()) {
        state.acted_this_street.assign(state.players.size(), false);
    }
    Player& p = state.players[static_cast<std::size_t>(ai)];
    const LegalActions legal = legal_actions_for(state, ai);

    switch (d.action) {
        case Action::Fold:
            p.folded = true;
            break;
        case Action::Check:
            if (!legal.can_check) {
                return false;
            }
            break;
        case Action::Call:
            // Calling with nothing to call is a check; a short stack calls all-in.
            put_in(state, p, std::min(legal.to_call, p.stack));
            break;
        case Action::Raise: {
            if (!legal.can_raise) {
                return false;
            }
            // `raise_by` is the increment above the call. It is raised to the minimum legal raise
            // and capped at all-in, which may be a short raise.
            const int full = full_raise_size(state);
            const std::int64_t wanted =
                static_cast<std::int64_t>(state.current_bet) + std::max(d.raise_by, full);
            const int raise_to = static_cast<int>(std::clamp<std::int64_t>(
                wanted, legal.min_raise_to, legal.max_raise_to));
            const int increment = raise_to - state.current_bet;
            put_in(state, p, raise_to - p.committed_this_street);
            if (increment >= full) {
                // A full raise reopens the betting for everyone and sets the new minimum raise.
                state.last_raise_increment = increment;
                state.acted_this_street.assign(state.players.size(), false);
            }
            state.current_bet = raise_to;
            break;
        }
    }
    state.acted_this_street[static_cast<std::size_t>(ai)] = true;
    state.current_bet = std::max(state.current_bet, max_committed_among_active(state));

    if (betting_round_complete(state)) {
        advance_phase_if_ready(state);
    } else {
        state.acting_index = next_actor_from(state, (ai + 1) % static_cast<int>(state.players.size()));
    }
    return true;
}

}  // namespace poker
