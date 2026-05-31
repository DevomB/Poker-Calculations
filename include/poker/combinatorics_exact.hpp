#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/range.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace poker {

struct HeroRunoutVulnerabilityResult {
    double p_nuts{0.0};
    double p_dominated{0.0};
    std::uint64_t runout_count{0};
};

struct VillainLeapfrogResult {
    std::vector<int> leapfrog_deck_indices;
    std::vector<int> hero_improve_deck_indices;
};

struct HeroEquityRunoutQuantilesResult {
    double mean{0.0};
    double variance{0.0};
    double p05{0.0};
    double p50{0.0};
    double p95{0.0};
    std::uint64_t n{0};
};

struct CardRemovalGradientResult {
    std::array<double, 52> gradient{};
    double base_equity{0.0};
};

[[nodiscard]] HeroRunoutVulnerabilityResult exact_hero_runout_vulnerability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel = nullptr);

[[nodiscard]] VillainLeapfrogResult exact_villain_leapfrog_out_counts(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel = nullptr);

[[nodiscard]] std::vector<double> exact_hero_category_joint_flop_to_river(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel = nullptr);

[[nodiscard]] double exact_range_dominated_combo_fraction(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, const CancelPredicate* cancel = nullptr);

[[nodiscard]] HeroEquityRunoutQuantilesResult exact_hero_equity_runout_quantiles_vs_random(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel = nullptr);

[[nodiscard]] HeroEquityRunoutQuantilesResult exact_hero_equity_runout_quantiles_vs_range(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, const CancelPredicate* cancel = nullptr);

[[nodiscard]] CardRemovalGradientResult exact_equity_card_removal_gradient(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, const CancelPredicate* cancel = nullptr);

[[nodiscard]] double exact_information_regret_vs_clairvoyant(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, double pot_before_call, double to_call,
    const CancelPredicate* cancel = nullptr);

}  // namespace poker
