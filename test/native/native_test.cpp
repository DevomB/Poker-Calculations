// C++-only checks: GameEngine no-limit betting rules (not exposed to JavaScript) and the shared
// 1326-combo / 169-class index helpers. Build and run: npm run test:native

#include "poker/equity_matrix.hpp"
#include "poker/game_engine.hpp"
#include "poker/nash_push_fold.hpp"
#include "poker/range.hpp"
#include "poker/short_deck.hpp"

#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                                 \
        }                                                                                 \
    } while (0)

using poker::Action;
using poker::Decision;
using poker::GameEngine;
using poker::GameEngineConfig;
using poker::GamePhase;
using poker::PokerGameState;

PokerGameState table(const std::vector<int>& stacks) {
    PokerGameState s;
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        poker::Player p;
        p.seat = static_cast<int>(i);
        p.stack = stacks[i];
        s.players.push_back(p);
    }
    return s;
}

int index_of_seat(const PokerGameState& s, int seat) {
    for (std::size_t i = 0; i < s.players.size(); ++i) {
        if (s.players[i].seat == seat) return static_cast<int>(i);
    }
    return -1;
}

long long chips(const PokerGameState& s) {
    long long total = s.pot;
    for (const auto& p : s.players) total += p.stack;
    return total;
}

bool act(GameEngine& e, PokerGameState& s, int seat, Action a, int raise_by = 0) {
    if (s.acting_index != index_of_seat(s, seat)) {
        std::fprintf(stderr, "expected seat %d to act, got index %d\n", seat, s.acting_index);
        ++g_failures;
        return false;
    }
    return e.apply_action(s, Decision{a, raise_by});
}

const poker::Player& seat(const PokerGameState& s, int seat_no) {
    return s.players[static_cast<std::size_t>(index_of_seat(s, seat_no))];
}

void heads_up_button_posts_small_blind_and_acts_first() {
    GameEngine e(1);
    PokerGameState s = table({100, 100});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(seat(s, 0).committed_this_street == 1);  // button = small blind
    CHECK(seat(s, 1).committed_this_street == 2);
    CHECK(s.current_bet == 2);
    CHECK(s.acting_index == index_of_seat(s, 0));  // button acts first preflop
    CHECK(act(e, s, 0, Action::Call));
    CHECK(act(e, s, 1, Action::Check));
    CHECK(s.phase == GamePhase::Flop);
    CHECK(s.community_cards.size() == 3);
    CHECK(s.acting_index == index_of_seat(s, 1));  // big blind acts first after the flop
}

void short_all_in_raise_does_not_reopen_betting() {
    // Seats 0 BTN, 1 SB, 2 BB; seat 0 has 150 behind after limping.
    GameEngine e(2);
    PokerGameState s = table({152, 1000, 1000});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(act(e, s, 0, Action::Call));
    CHECK(act(e, s, 1, Action::Call));
    CHECK(act(e, s, 2, Action::Check));
    CHECK(s.phase == GamePhase::Flop);
    CHECK(act(e, s, 1, Action::Raise, 100));  // bet 100
    CHECK(act(e, s, 2, Action::Call));
    CHECK(act(e, s, 0, Action::Raise, 1000));  // all-in to 150: a 50 raise, short of 100
    CHECK(seat(s, 0).stack == 0);
    CHECK(s.current_bet == 150);
    CHECK(s.last_raise_increment == 100);  // the last full raise is kept
    const int sb = index_of_seat(s, 1);
    CHECK(s.acting_index == sb);
    const poker::LegalActions legal = poker::legal_actions_for(s, sb);
    CHECK(legal.can_call && legal.can_fold && !legal.can_raise);
    CHECK(!e.apply_action(s, Decision{Action::Raise, 500}));  // not reopened
    CHECK(act(e, s, 1, Action::Call));
    CHECK(act(e, s, 2, Action::Call));
    CHECK(s.phase == GamePhase::Turn);
    CHECK(s.pot == 6 + 3 * 150);
}

void short_all_ins_that_add_up_to_a_full_raise_reopen_betting() {
    // Seats 0 BTN, 1 SB, 2 BB, 3 UTG.
    GameEngine e(3);
    PokerGameState s = table({202, 1000, 1000, 152});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(act(e, s, 3, Action::Call));
    CHECK(act(e, s, 0, Action::Call));
    CHECK(act(e, s, 1, Action::Call));
    CHECK(act(e, s, 2, Action::Check));
    CHECK(act(e, s, 1, Action::Raise, 100));
    CHECK(act(e, s, 2, Action::Call));
    CHECK(act(e, s, 3, Action::Raise, 1000));  // all-in to 150 (short)
    const poker::LegalActions btn = poker::legal_actions_for(s, index_of_seat(s, 0));
    CHECK(btn.can_raise);                // seat 0 has not acted this street
    CHECK(btn.min_raise_to == 200);      // 150 + 100 would be 250; it only has 200
    CHECK(act(e, s, 0, Action::Raise, 1000));  // all-in to 200 (short again)
    CHECK(s.last_raise_increment == 100);
    // Seat 1 bet 100 and now faces 100 more: together the short raises are a full raise.
    CHECK(poker::legal_actions_for(s, index_of_seat(s, 1)).can_raise);
    CHECK(act(e, s, 1, Action::Raise, 100));  // to 300
    CHECK(s.last_raise_increment == 100);
    CHECK(s.current_bet == 300);
}

void short_big_blind_still_sets_the_full_bet() {
    // Button seat 1: SB seat 2, BB seat 0 (one chip), first to act seat 1.
    GameEngine e(4);
    PokerGameState s = table({1, 100, 100});
    e.start_new_hand(s, GameEngineConfig{1, 2, 1});
    CHECK(seat(s, 0).committed_this_street == 1);
    CHECK(seat(s, 0).stack == 0);
    CHECK(s.current_bet == 2);
    CHECK(s.acting_index == index_of_seat(s, 1));
    CHECK(s.to_call(seat(s, 1)) == 2);
    CHECK(act(e, s, 1, Action::Call));
    CHECK(act(e, s, 2, Action::Call));  // small blind completes to 2
    CHECK(seat(s, 2).total_committed_hand == 2);
    CHECK(s.phase == GamePhase::Flop);
}

void last_player_standing_ends_the_hand() {
    GameEngine e(5);
    PokerGameState s = table({100, 100, 100});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(act(e, s, 0, Action::Raise, 4));
    CHECK(act(e, s, 1, Action::Fold));
    CHECK(act(e, s, 2, Action::Fold));
    CHECK(s.phase == GamePhase::HandComplete);
    CHECK(s.acting_index == -1);
    CHECK(s.community_cards.empty());
    CHECK(!e.apply_action(s, Decision{Action::Check, 0}));
}

void all_in_and_call_runs_the_board_out() {
    GameEngine e(6);
    PokerGameState s = table({100, 60});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(act(e, s, 0, Action::Raise, 1000));
    CHECK(act(e, s, 1, Action::Call));
    CHECK(s.phase == GamePhase::Showdown);
    CHECK(s.community_cards.size() == 5);
    CHECK(s.acting_index == -1);
    CHECK(chips(s) == 160);
}

void covering_player_is_not_asked_to_act_alone() {
    // Seat 1 (BB) calls all-in for less; the button cannot bet into nobody on later streets.
    GameEngine e(7);
    PokerGameState s = table({100, 30});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(act(e, s, 0, Action::Raise, 18));  // to 20
    CHECK(act(e, s, 1, Action::Raise, 1000));  // all-in to 30 (short)
    CHECK(act(e, s, 0, Action::Call));
    CHECK(s.phase == GamePhase::Showdown);
    CHECK(s.community_cards.size() == 5);
}

void too_few_funded_players_does_not_start_a_hand() {
    GameEngine e(8);
    PokerGameState s = table({100, 0});
    e.start_new_hand(s, GameEngineConfig{1, 2, 0});
    CHECK(s.phase == GamePhase::HandComplete);
    CHECK(s.acting_index == -1);
    CHECK(chips(s) == 100);
}

void random_play_conserves_chips_and_terminates() {
    std::mt19937 rng(20261104);
    for (int hand = 0; hand < 3000; ++hand) {
        const int n = 2 + static_cast<int>(rng() % 5);
        std::vector<int> stacks(static_cast<std::size_t>(n));
        for (int& st : stacks) st = 1 + static_cast<int>(rng() % 400);
        PokerGameState s = table(stacks);
        const long long total = std::accumulate(stacks.begin(), stacks.end(), 0LL);
        GameEngine e(rng());
        e.start_new_hand(s, GameEngineConfig{1 + static_cast<int>(rng() % 3), 4, static_cast<int>(rng() % n)});
        CHECK(chips(s) == total);
        int steps = 0;
        while (s.acting_index >= 0 && steps < 400) {
            const poker::LegalActions legal = poker::legal_actions_for(s, s.acting_index);
            CHECK(legal.can_check || legal.can_call);
            const unsigned r = rng() % 10;
            Decision d{Action::Check, 0};
            if (r < 2 && legal.can_fold) d.action = Action::Fold;
            else if (r < 4 && legal.can_raise) d = Decision{Action::Raise, static_cast<int>(rng() % 120)};
            else d.action = legal.can_check ? Action::Check : Action::Call;
            CHECK(e.apply_action(s, d));
            CHECK(chips(s) == total);
            long long committed = 0;
            for (const auto& p : s.players) {
                CHECK(p.stack >= 0);
                committed += p.total_committed_hand;
            }
            CHECK(committed == s.pot);
            ++steps;
        }
        CHECK(steps < 400);
        CHECK(s.phase == GamePhase::Showdown || s.phase == GamePhase::HandComplete);
        if (s.phase == GamePhase::Showdown) CHECK(s.community_cards.size() == 5);
        if (s.phase == GamePhase::HandComplete) CHECK(s.active_players_in_hand() == 1);
    }
}

void combo_index_covers_every_pair_in_both_orders() {
    // The dense order is the nested a < b loop that sparse_range_from_dense1326 walks.
    int expected = 0;
    for (int a = 0; a < 52; ++a) {
        for (int b = a + 1; b < 52; ++b) {
            CHECK(poker::combo_index_1326(a, b) == expected);
            CHECK(poker::combo_index_1326(b, a) == expected);
            CHECK(poker::combo_index_1326_unchecked(b, a) == expected);
            const auto [x, y] = poker::combo_cards_1326(expected);
            CHECK(x == a && y == b);
            ++expected;
        }
    }
    CHECK(expected == poker::kHoleComboCount);
    CHECK(poker::combo_index_1326(5, 5) == -1);
    CHECK(poker::combo_index_1326(-1, 3) == -1);
    CHECK(poker::combo_index_1326(3, 52) == -1);
    CHECK(poker::combo_cards_1326(-1).first == -1);
    CHECK(poker::combo_cards_1326(1326).first == -1);
}

void hand169_order_round_trips() {
    // Reference: the loop the matrix, Nash, and short-deck code each used to carry.
    int idx = 0;
    for (int low = 0; low < 13; ++low) {
        for (int high = low; high < 13; ++high) {
            for (const bool suited : {true, false}) {
                if (high == low && suited) continue;
                const poker::HandClass169 c = poker::hand169_class(idx);
                CHECK(c.high_rank == high && c.low_rank == low && c.suited == suited);
                CHECK(poker::hand169_index(high, low, suited) == idx);
                CHECK(poker::hand169_index(low, high, suited) == idx);
                CHECK(poker::nash_hand169_index(high, low, suited) == idx);
                const int sd = poker::short_deck_hand81_from_hand169(idx);
                CHECK((sd >= 0) == (low >= poker::kShortDeckMinRank));
                ++idx;
            }
        }
    }
    CHECK(idx == poker::kHandClassCount);
    CHECK(poker::nash_hand169_from_notation("22") == 0);
    CHECK(poker::nash_hand169_from_notation("32s") == 1);
    CHECK(poker::nash_hand169_from_notation("32o") == 2);
    CHECK(poker::nash_hand169_from_notation("AKo") == 167);
    CHECK(poker::nash_hand169_from_notation("AA") == 168);
    bool threw = false;
    try {
        (void)poker::hand169_class(169);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

}  // namespace

int main() {
    combo_index_covers_every_pair_in_both_orders();
    hand169_order_round_trips();
    heads_up_button_posts_small_blind_and_acts_first();
    short_all_in_raise_does_not_reopen_betting();
    short_all_ins_that_add_up_to_a_full_raise_reopen_betting();
    short_big_blind_still_sets_the_full_bet();
    last_player_standing_ends_the_hand();
    all_in_and_call_runs_the_board_out();
    covering_player_is_not_asked_to_act_alone();
    too_few_funded_players_does_not_start_a_hand();
    random_play_conserves_chips_and_terminates();
    if (g_failures != 0) {
        std::fprintf(stderr, "native_test: %d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("OK: native_test\n");
    return EXIT_SUCCESS;
}
