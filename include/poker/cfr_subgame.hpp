#pragma once

#include "poker/card.hpp"
#include "poker/range.hpp"

#include <string>
#include <vector>

namespace poker {

/// Regret matching: max(r,0)/sum; uniform if all regrets ≤ 0.
[[nodiscard]] std::vector<double> regret_matching_strategy(const std::vector<double>& regrets);

struct CfrNodeUpdate {
    std::vector<double> regrets;
    std::vector<double> strategy;
};

/// One CFR info-set update: `regret[a] += reach * instantaneous[a]`, then regret-match.
[[nodiscard]] CfrNodeUpdate cfr_node_reach_update(const std::vector<double>& cumulative_regrets,
                                                  const std::vector<double>& instantaneous_regrets,
                                                  double reach);

struct StrategySupportSize {
    int mixed_count{0};
    double pure_mass{0.0};
};

/// `mixedCount` = entries in `(eps, 1-eps)`; `pureMass` = mean of entries `≥ 1-eps`.
[[nodiscard]] StrategySupportSize strategy_support_size(const std::vector<double>& action_probs,
                                                        double eps = 0.02);

/**
 * HU river tree (chip EV from the posted pot):
 * - Bettor: Check or Bet (`bet_size`).
 * - Check → defender checks back, showdown for `pot`.
 * - Bet → defender Fold (bettor takes `pot`) or Call (showdown for `pot + 2*bet`, each pays `bet`).
 * Showdown uses exact 7-card compare (`evaluate_hand_strength_fast`).
 */
struct RiverCfrResult {
    double bet_freq{0.0};
    double call_freq{0.0};
    double ev_bettor{0.0};
    double ev_defender{0.0};
    int iterations{0};
    std::vector<double> bet_mix_1326;
    std::vector<double> call_mix_1326;
};

[[nodiscard]] RiverCfrResult cfr_river_bet_call_fold_solve(double pot, double bet_size,
                                                           const SparseRange& bettor_range,
                                                           const SparseRange& defender_range,
                                                           const std::vector<Card>& board,
                                                           int iterations = 400);

[[nodiscard]] RiverCfrResult fictitious_play_river(double pot, double bet_size,
                                                   const SparseRange& bettor_range,
                                                   const SparseRange& defender_range,
                                                   const std::vector<Card>& board,
                                                   int iterations = 400);

struct RiverProfileEv {
    double ev_bettor{0.0};
    double ev_defender{0.0};
};

[[nodiscard]] RiverProfileEv ev_of_strategy_profile(double pot, double bet_size,
                                                    const SparseRange& bettor_range,
                                                    const SparseRange& defender_range,
                                                    const std::vector<Card>& board,
                                                    const std::vector<double>& bettor_mix,
                                                    const std::vector<double>& caller_mix);

struct BestResponseRiverResult {
    double value{0.0};
    double call_frequency{0.0};
    std::string action;
};

/// Hero is the defender. `villain_bet_mix` is a scalar (length 1) or per-1326 bet frequencies.
[[nodiscard]] BestResponseRiverResult best_response_river(double pot, double bet_size,
                                                          const SparseRange& hero_range,
                                                          const SparseRange& villain_range,
                                                          const std::vector<Card>& board,
                                                          const std::vector<double>& villain_bet_mix);

/// NashConv = 0.5 * (BR_bettor + BR_defender − EV_bettor − EV_defender). ≥ 0 at a Nash profile.
[[nodiscard]] double exploitability_river(double pot, double bet_size, const SparseRange& bettor_range,
                                          const SparseRange& defender_range, const std::vector<Card>& board,
                                          const std::vector<double>& bettor_mix,
                                          const std::vector<double>& caller_mix);

/**
 * HU preflop jam/fold vs call/fold. Chip EV relative to starting stack (BB units).
 * Blinds 0.5 / 1.0. Jammer first: Fold (−0.5) or Jam; caller Fold (−1 for jammer +1) or Call (all-in).
 * Called equity is Monte Carlo vs the opposing hole (`simulate_hand_outcome_vs_villain_holes`).
 */
struct PushFoldCfrResult {
    double jam_freq{0.0};
    double call_freq{0.0};
    double ev_jammer{0.0};
    double ev_caller{0.0};
    int iterations{0};
    std::vector<double> jam_mix_1326;
    std::vector<double> call_mix_1326;
};

[[nodiscard]] PushFoldCfrResult cfr_heads_up_push_fold_solve(const SparseRange& jammer_range,
                                                             const SparseRange& caller_range,
                                                             double stack_bb, int iterations = 400);

struct RiverClassFreqs {
    double air_bet{0.0};
    double draw_bet{0.0};
    double made_bet{0.0};
    double strong_bet{0.0};
    double air_call{0.0};
    double draw_call{0.0};
    double made_call{0.0};
    double strong_call{0.0};
};

struct TopBetCombo {
    int combo_index{0};
    int card_a{0};
    int card_b{0};
    double bet_frequency{0.0};
    double weight{0.0};
};

struct HuRiverCheckBetTreeResult {
    RiverCfrResult solve;
    RiverClassFreqs classes;
    std::vector<TopBetCombo> top_bet_combos;
};

[[nodiscard]] HuRiverCheckBetTreeResult solve_hu_river_check_bet_tree(double pot, double bet_size,
                                                                      const SparseRange& bettor_range,
                                                                      const SparseRange& defender_range,
                                                                      const std::vector<Card>& board,
                                                                      int iterations = 400, int top_k = 8);

}  // namespace poker
