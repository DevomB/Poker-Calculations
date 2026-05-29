#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/range.hpp"

#include <random>

namespace poker {

struct McEquityDetailedResult {
    double estimate{0.0};
    double se{0.0};
    double ci_low{0.0};
    double ci_high{0.0};
    int n{0};
};

[[nodiscard]] McEquityDetailedResult simulate_hand_outcome_detailed(
    const std::vector<Card>& player_hand, const std::vector<Card>& community_cards,
    int num_simulations, std::mt19937& rng, int villains = 1,
    const CancelPredicate* cancel = nullptr);

[[nodiscard]] float simulate_equity_vs_range(const std::vector<Card>& player_hand,
                                             const std::vector<Card>& community_cards,
                                             const SparseRange& villain_range, int num_simulations,
                                             std::mt19937& rng, const CancelPredicate* cancel = nullptr);

}  // namespace poker
