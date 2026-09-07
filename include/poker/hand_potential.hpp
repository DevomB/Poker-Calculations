#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/range.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace poker {

/// HS / PPot / NPot / EHS / EHS2 vs a villain range on a flop or turn.
/// Ties use the HU chop (half) from NUMERICAL.md — Billings / poker-eval / Casinostates.
struct HandPotentialBreakdown {
    double hs{0.0};
    double ppot{0.0};
    double npot{0.0};
    double ehs{0.0};
    double ehs2{0.0};
    double n_behind{0.0};
    double n_ahead{0.0};
    double n_tied{0.0};
};

struct ComboEhsTableOptions {
    /// 0 = exact next-street enumeration. Non-zero = Monte Carlo runouts (flop tables).
    std::size_t trials{0};
    std::uint32_t seed{0};
};

enum class PotentialStreets { One = 1, Two = 2 };

[[nodiscard]] HandPotentialBreakdown hand_potential_breakdown(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, PotentialStreets streets = PotentialStreets::One,
    const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double hand_strength_vs_range(const std::vector<Card>& hero_hole_cards,
                                            const std::vector<Card>& board_cards,
                                            const SparseRange& villain_range,
                                            const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double positive_potential_vs_range(const std::vector<Card>& hero_hole_cards,
                                                 const std::vector<Card>& board_cards,
                                                 const SparseRange& villain_range,
                                                 const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double negative_potential_vs_range(const std::vector<Card>& hero_hole_cards,
                                                 const std::vector<Card>& board_cards,
                                                 const SparseRange& villain_range,
                                                 const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double effective_hand_strength(const std::vector<Card>& hero_hole_cards,
                                             const std::vector<Card>& board_cards,
                                             const SparseRange& villain_range,
                                             const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double effective_hand_strength_squared(const std::vector<Card>& hero_hole_cards,
                                                     const std::vector<Card>& board_cards,
                                                     const SparseRange& villain_range,
                                                     const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double two_street_positive_potential(const std::vector<Card>& hero_hole_cards,
                                                   const std::vector<Card>& board_cards,
                                                   const SparseRange& villain_range,
                                                   const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double two_street_negative_potential(const std::vector<Card>& hero_hole_cards,
                                                   const std::vector<Card>& board_cards,
                                                   const SparseRange& villain_range,
                                                   const CancelPredicate* should_cancel = nullptr);

/// Equal-width bucket in `[0, k)` for `ehs` in `[0, 1]`.
[[nodiscard]] int equity_bucket_from_ehs(double ehs, int bucket_count);

/// EHS for every hero 1326 combo (0 if blocked by the board). Turn is exact when
/// `trials == 0`. Flop exact is allowed but heavy; pass `trials` to Monte Carlo runouts.
[[nodiscard]] std::vector<double> combo_ehs_table_vs_range(
    const std::vector<Card>& board_cards, const SparseRange& villain_range,
    const ComboEhsTableOptions& options = {}, const CancelPredicate* should_cancel = nullptr);

}  // namespace poker
