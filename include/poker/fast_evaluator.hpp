#pragma once

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace poker {

/// In-house stack-only evaluator for Monte Carlo / exact enumeration hot paths.
/// Produces the same strength encoding as `evaluate_hand_strength`.
struct EvaluatorBenchmarkResult {
    double legacy_evals_per_second{0.0};
    double fast_evals_per_second{0.0};
    const char* implementation{"poker-calculations-forge"};
};

[[nodiscard]] std::uint64_t pack_hand_strength(const HandEvaluation& evaluation);

[[nodiscard]] HandEvaluation evaluate_five_cards_fast(const std::uint8_t ranks[5],
                                                      const std::uint8_t suits[5]);

[[nodiscard]] HandEvaluation evaluate_best_hand_fast(const std::uint8_t ranks[],
                                                     const std::uint8_t suits[],
                                                     int card_count);

[[nodiscard]] std::uint64_t evaluate_hand_strength_fast(const std::vector<Card>& player_hand,
                                                        const std::vector<Card>& community_cards);

[[nodiscard]] std::uint64_t evaluate_seven_strength_fast(const std::uint8_t ranks[7],
                                                         const std::uint8_t suits[7]);

/// Returns `-1` / `0` / `1` comparing best seven-card strengths (hole + board runout).
[[nodiscard]] int compare_seven_strength_fast(const std::uint8_t ranks_a[7],
                                            const std::uint8_t suits_a[7],
                                            const std::uint8_t ranks_b[7],
                                            const std::uint8_t suits_b[7]);

[[nodiscard]] EvaluatorBenchmarkResult benchmark_evaluator_throughput(
    std::size_t iterations = 200000);

}  // namespace poker
