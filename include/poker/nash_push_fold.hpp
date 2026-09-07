#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace poker {

/// Canonical 169 order matches `build_preflop_equity_matrix` / `hand169_to_deck_indices`.
inline constexpr int kNashHandCount = 169;
inline constexpr int kNashDefaultIterations = 50;
inline constexpr int kNashMaxIterations = 80;
inline constexpr double kNashDefaultTolerance = 1e-3;
inline constexpr int kNashDefaultEquityIterations = 80;
inline constexpr double kNashDefaultMaxStackBb = 40.0;

/**
 * Two-player jam/fold vs call/fold Nash (fictitious play).
 *
 * Iteration cap: `max_iterations` (default 50, hard cap 80).
 * Indifference band: `|EV| < tolerance` (default 1e-3) mixes 0.5.
 * Hands are 169 buckets with combo weights 22=6, AKs=4, AKo=12 (blockers ignored).
 * Equities come from a cached 169×169 Monte Carlo matrix in the same order as
 * `buildPreflopEquityMatrix`.
 *
 * Chip EV is measured after blinds/antes are in the pot: fold = 0, jam-fold = +pot.
 * ICM uses Harville `icm_expected_payouts` on terminal stacks; busted stacks are
 * floored at 1e-6 so Harville weights stay defined. Showdown ties split as
 * `eq` / `1-eq` (no explicit chop stack vector).
 */
struct NashPushFoldSpec {
    double small_blind{0.5};
    double big_blind{1.0};
    double ante{0.0};
    double hero_stack{10.0};
    double villain_stack{10.0};
    double hero_posted{0.5};
    double villain_posted{1.0};
    int max_iterations{kNashDefaultIterations};
    double tolerance{kNashDefaultTolerance};
    int equity_iterations{kNashDefaultEquityIterations};
    std::uint32_t equity_seed{1};
    bool use_icm{false};
    std::vector<double> other_stacks;
    std::vector<double> payouts;
};

struct NashJamCallResult {
    std::array<double, kNashHandCount> jam{};
    std::array<double, kNashHandCount> call{};
    double hero_ev{0.0};
    double villain_ev{0.0};
    int iterations{0};
};

struct NashMultiwayResult {
    std::array<double, kNashHandCount> jam{};
    std::vector<std::array<double, kNashHandCount>> calls;
    int iterations{0};
};

[[nodiscard]] int nash_hand169_index(int high_rank, int low_rank, bool suited);
[[nodiscard]] int nash_hand169_from_notation(const std::string& notation);
[[nodiscard]] double nash_combo_weight(int hand169);

[[nodiscard]] NashJamCallResult nash_heads_up_jam_call_solve(const NashPushFoldSpec& spec);

[[nodiscard]] NashMultiwayResult nash_multiway_shove_call(const NashPushFoldSpec& spec,
                                                          const std::vector<double>& caller_stacks);

[[nodiscard]] std::array<double, kNashHandCount> nash_jam_threshold_stack_bb(
    const NashPushFoldSpec& spec, double max_stack_bb);

[[nodiscard]] std::array<double, kNashHandCount> nash_call_threshold_stack_bb(
    const NashPushFoldSpec& spec, double max_stack_bb);

[[nodiscard]] double nash_indifference_stack_bb(int hand169, const NashPushFoldSpec& spec,
                                                double max_stack_bb);

}  // namespace poker
