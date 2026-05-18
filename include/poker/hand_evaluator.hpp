#pragma once

#include "poker/card.hpp"
#include "poker/types.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace poker {

struct HandEvaluation {
    HandRank rank{HandRank::HighCard};
    /// Comparison order: most significant first (e.g. top pair rank, then kicker...).
    std::array<std::uint8_t, 5> kickers{};

    [[nodiscard]] bool operator<(const HandEvaluation& o) const;
    [[nodiscard]] bool operator==(const HandEvaluation& o) const;
};

[[nodiscard]] HandEvaluation evaluate_five_cards(const std::vector<Card>& five);

/// Best 5-card hand from 1–7 distinct cards (undefined if size not in [1,7]).
[[nodiscard]] HandEvaluation evaluate_best_hand(const std::vector<Card>& cards);

[[nodiscard]] HandRank hand_category(const HandEvaluation& e);

/// Monotonic scalar: higher means strictly stronger (ties map to same value).
[[nodiscard]] std::uint64_t evaluate_hand_strength(const std::vector<Card>& player_hand,
                                                   const std::vector<Card>& community_cards);

/// Plan-style API: category of the best 7-card combination.
[[nodiscard]] HandRank evaluate_hand(const std::vector<Card>& hand,
                                     const std::vector<Card>& community_cards);

/**
 * Compare best 1–7 card hands (no board mixing). Returns `-1` if `a` loses to `b`, `0` tie, `1` if `a`
 * wins. Throws if lengths invalid, duplicate cards within `a` or `b`, or any card appears in both.
 */
[[nodiscard]] int compare_best_hands(const std::vector<Card>& a, const std::vector<Card>& b);

}  // namespace poker
