#pragma once

#include "poker/card.hpp"
#include "poker/range.hpp"

#include <array>
#include <string>
#include <vector>

namespace poker {

struct MaterializedRangeResult {
    std::array<double, 1326> weights{};
    int live_combo_count{0};
    double weight_sum{0.0};
    double shannon_entropy{0.0};
};

[[nodiscard]] MaterializedRangeResult materialize_villain_range_after_blockers(
    const double* dense1326, std::size_t dense_len, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, const std::vector<Card>& known_dead_cards);

[[nodiscard]] MaterializedRangeResult materialize_villain_range_after_blockers_sparse(
    const SparseRange& prior, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, const std::vector<Card>& known_dead_cards);

enum class BayesianActionKind { Fold, Call, Raise };

[[nodiscard]] MaterializedRangeResult bayesian_range_update_from_action(
    const SparseRange& prior, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, BayesianActionKind action, double alpha);

}  // namespace poker
