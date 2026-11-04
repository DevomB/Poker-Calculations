#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace poker {

/// Hero's pot share at showdown. `strengths[0]` is hero; the rest are opponents. Hero receives
/// `1 / k` when tied k ways for the best strength, otherwise `0` (NUMERICAL.md, showdown equity).
[[nodiscard]] inline double hero_showdown_share(const std::vector<std::uint64_t>& strengths) noexcept {
    if (strengths.empty()) {
        return 0.0;
    }
    const std::uint64_t hero = strengths[0];
    int tied_at_best = 1;
    for (std::size_t i = 1; i < strengths.size(); ++i) {
        if (strengths[i] > hero) {
            return 0.0;
        }
        if (strengths[i] == hero) {
            ++tied_at_best;
        }
    }
    return 1.0 / static_cast<double>(tied_at_best);
}

/// Throws std::invalid_argument unless hero has 2 cards, the board has at most 5, no card repeats,
/// and the deck still holds 2 cards per villain plus the rest of the board.
void validate_holdem_spot(const std::vector<Card>& player_hand,
                          const std::vector<Card>& community_cards, int villains);

/// Estimated equity (0–1) for hero winning share of a multi-way pot at showdown.
[[nodiscard]] float simulate_hand_outcome(const std::vector<Card>& player_hand,
                                          const std::vector<Card>& community_cards,
                                          int num_simulations,
                                          std::mt19937& rng,
                                          int villains = 1,
                                          const CancelPredicate* cancel = nullptr);

/**
 * Hero's mean showdown share against nested sets of random opponents, from the same deals: each
 * trial deals `max(opponent_counts)` random hands and completes the board, and `out[i]` averages
 * hero's share against the first `opponent_counts[i]` of them (1 when that count is 0). Used to
 * price side-pot layers, whose eligible players are nested.
 */
[[nodiscard]] std::vector<double> simulate_nested_showdown_shares(const std::vector<Card>& player_hand,
                                                                  const std::vector<Card>& community_cards,
                                                                  const std::vector<int>& opponent_counts,
                                                                  int num_simulations, std::mt19937& rng,
                                                                  const CancelPredicate* cancel = nullptr);

/// Same as repeated `simulate_hand_outcome`, split across `num_threads` workers.
[[nodiscard]] float parallel_hand_simulation(const std::vector<Card>& player_hand,
                                             const std::vector<Card>& community_cards,
                                             int num_simulations,
                                             std::uint32_t base_seed,
                                             int villains,
                                             std::size_t num_threads,
                                             const CancelPredicate* cancel = nullptr);

struct SimSpot {
    std::vector<Card> hole;
    std::vector<Card> board;
    int num_simulations{0};
    std::uint32_t seed{0};
    int villains{1};
};

void simulate_hand_outcome_batch(const std::vector<SimSpot>& spots, std::vector<float>& out_equities);

/// MC equity vs fixed villain hole cards (deck indices 0..51).
[[nodiscard]] float simulate_hand_outcome_vs_villain_holes(
    const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
    int villain_deck_a, int villain_deck_b, int num_simulations, std::mt19937& rng,
    const CancelPredicate* cancel = nullptr);

}  // namespace poker
