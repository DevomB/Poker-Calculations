#pragma once

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace poker {

/// Ace-to-5 low (razz). Aces low. Straights and flushes do not count.
/// `operator<` is **lower-better** (wheel is the minimum).
struct RazzEvaluation {
    HandRank rank{HandRank::HighCard};
    std::array<std::uint8_t, 5> kickers{};

    [[nodiscard]] bool operator<(const RazzEvaluation& o) const;
    [[nodiscard]] bool operator==(const RazzEvaluation& o) const;
};

/// Best 5-card high from **3–7** stud cards. Thin wrap of `evaluate_best_hand`.
[[nodiscard]] HandEvaluation evaluate_stud_best_hand(const std::vector<Card>& cards);

/// Best 5-card A-5 low from **3–7** cards. Lower `pack_razz_strength` is better.
[[nodiscard]] RazzEvaluation evaluate_razz_hand(const std::vector<Card>& cards);

[[nodiscard]] std::uint64_t pack_razz_strength(const RazzEvaluation& e);

/// True iff the best 5 is A2345 (wheel). Suited wheel still counts — flushes do not.
[[nodiscard]] bool razz_wheel_is_nuts(const std::vector<Card>& cards);

/// Exact HU pot-share (ties 0.5). Each hand is 3–7 cards; remaining streets are dealt
/// from the dead-aware deck (`extra_dead` = folded upcards). Both-7 compares once.
[[nodiscard]] double exact_hu_stud_equity_vs_known(const std::vector<Card>& hero,
                                                   const std::vector<Card>& villain,
                                                   const std::vector<Card>& extra_dead = {});

[[nodiscard]] double exact_hu_razz_equity_vs_known(const std::vector<Card>& hero,
                                                   const std::vector<Card>& villain,
                                                   const std::vector<Card>& extra_dead = {});

/// Remaining deck after holes + upcards (unique dead). Deck-index order, canonical strings.
[[nodiscard]] std::vector<std::string> stud_dead_card_deck(const std::vector<Card>& dead);

[[nodiscard]] float simulate_stud_equity_vs_random(const std::vector<Card>& hero, int trials, std::mt19937& rng,
                                                   const std::vector<Card>& extra_dead = {});

[[nodiscard]] float simulate_razz_equity_vs_random(const std::vector<Card>& hero, int trials, std::mt19937& rng,
                                                   const std::vector<Card>& extra_dead = {});

}  // namespace poker
