#pragma once

#include "poker/card.hpp"
#include "poker/range.hpp"
#include "poker/types.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace poker {

/**
 * Short Deck / 6+ Hold'em (36-card).
 *
 * Deck: ranks **6–A** only. Card encoding matches NLHE (`rank` 0=2 .. 12=A, deck id
 * `rank * 4 + suit`); ranks 2–5 (`rank` 0–3, deck ids 0–15) are invalid and rejected.
 *
 * Straight: **A-6-7-8-9** is the wheel (high = 9). A-2-3-4-5 does not exist.
 * Broadway T-J-Q-K-A is unchanged. Suited AKQJT is a royal flush. Suited A6789 is a
 * nine-high straight flush (steel wheel), not a royal.
 *
 * Category order (PokerStars / GG 6+): flush **beats** full house.
 *   highCard < onePair < twoPair < threeOfAKind < straight < **fullHouse < flush**
 *   < fourOfAKind < straightFlush < royalFlush
 *
 * Do not reuse NLHE `HandRank` ordering for comparison: in NLHE flush < boat.
 */
inline constexpr int kShortDeckCardCount = 36;
inline constexpr int kShortDeckMinRank = 4;  // 6
inline constexpr int kShortDeckHandClasses = 81;  // 9 pairs + 36 suited + 36 offsuit (not 169, not 91)
inline constexpr int kShortDeckNashDefaultIterations = 50;
inline constexpr int kShortDeckNashMaxIterations = 80;
inline constexpr int kShortDeckNashDefaultEquityIterations = 24;

enum class ShortDeckRank {
    HighCard = 0,
    OnePair,
    TwoPair,
    ThreeOfAKind,
    Straight,
    FullHouse,
    Flush,
    FourOfAKind,
    StraightFlush,
    RoyalFlush,
};

struct ShortDeckEvaluation {
    ShortDeckRank rank{ShortDeckRank::HighCard};
    std::array<std::uint8_t, 5> kickers{};

    [[nodiscard]] bool operator<(const ShortDeckEvaluation& o) const;
    [[nodiscard]] bool operator==(const ShortDeckEvaluation& o) const;
};

struct ShortDeckNashSpec {
    double small_blind{0.5};
    double big_blind{1.0};
    double ante{0.0};
    double hero_stack{10.0};
    double villain_stack{10.0};
    double hero_posted{0.5};
    double villain_posted{1.0};
    int max_iterations{kShortDeckNashDefaultIterations};
    double tolerance{1e-3};
    int equity_iterations{kShortDeckNashDefaultEquityIterations};
    std::uint32_t equity_seed{1};
};

[[nodiscard]] bool is_short_deck_card(const Card& c);

/// Throws if any card is rank 2–5, duplicated, or (when `require_size`) length is outside `[min_n, max_n]`.
void require_short_deck_cards(const std::vector<Card>& cards, const char* ctx, int min_n, int max_n);

[[nodiscard]] ShortDeckEvaluation evaluate_short_deck_five(std::vector<Card> five);

/// Best five of 1–7 short-deck cards.
[[nodiscard]] ShortDeckEvaluation evaluate_short_deck_best_hand(const std::vector<Card>& cards);

[[nodiscard]] std::uint64_t evaluate_short_deck_hand_strength(const std::vector<Card>& player_hand,
                                                              const std::vector<Card>& community_cards);

[[nodiscard]] ShortDeckRank evaluate_short_deck_category(const std::vector<Card>& player_hand,
                                                         const std::vector<Card>& community_cards);

[[nodiscard]] const char* short_deck_rank_label(ShortDeckRank r);

/// NLHE label enum for interned JS strings (`flush` / `fullHouse` names unchanged; order is not).
[[nodiscard]] HandRank short_deck_rank_to_label(ShortDeckRank r);

/// True when five cards are the A6789 wheel (straight or straight flush).
[[nodiscard]] bool short_deck_straight_is_wheel(const std::vector<Card>& five);

/// `C(n, 2)` hole combos on the remaining 36-card deck after `dead` (2–5 rejected).
[[nodiscard]] int short_deck_remaining_combo_count(const std::vector<Card>& dead);

/// Exact HU equity (hero share, ties = 0.5) on the 36-card deck. Board length 0–5.
[[nodiscard]] double exact_hu_short_deck_equity_vs_known(const std::vector<Card>& hero_hole,
                                                         const std::vector<Card>& villain_hole,
                                                         const std::vector<Card>& board);

[[nodiscard]] double simulate_short_deck_equity_vs_random(const std::vector<Card>& hero_hole,
                                                          const std::vector<Card>& board,
                                                          int num_simulations, std::mt19937& rng);

[[nodiscard]] double simulate_short_deck_equity_vs_range(const std::vector<Card>& hero_hole,
                                                         const std::vector<Card>& board,
                                                         const SparseRange& villain_range,
                                                         int num_simulations, std::mt19937& rng);

/// 81-class weights. Classes use ranks 6–A only (pair / suited / offsuit, same nested order as 169).
[[nodiscard]] SparseRange short_deck_range_from_class_weights(const std::array<double, kShortDeckHandClasses>& weights,
                                                              std::uint64_t dead_mask);

[[nodiscard]] int short_deck_hand81_index(int high_rank, int low_rank, bool suited);
[[nodiscard]] int short_deck_hand81_from_notation(const std::string& notation);
/// 169-class index → 81 if both ranks are 6–A; otherwise `-1`.
[[nodiscard]] int short_deck_hand81_from_hand169(int hand169);

/// Jam frequencies (81), SB jam/fold vs BB call/fold using short-deck matchup equities.
[[nodiscard]] std::array<double, kShortDeckHandClasses> short_deck_nash_hu_jam_range(
    const ShortDeckNashSpec& spec);

/// True when NLHE vs 6+ **category labels** differ for the same 5–7 cards (wheel; not flush-vs-boat names).
[[nodiscard]] bool short_deck_vs_holdem_category_flip(const std::vector<Card>& cards);

}  // namespace poker
