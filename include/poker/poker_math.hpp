#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace poker {

/// `to_call / (pot + to_call)` when `to_call > 0` and `pot + to_call > 0`; else `0`.
[[nodiscard]] double pot_odds_ratio(int pot, int to_call);

/// Chip EV of calling once vs folding (0); ignores future streets.
[[nodiscard]] double expected_value_call(double equity, int pot, int to_call);

// --- Chip / odds helpers (ported from former poker-math.js) ---

[[nodiscard]] double spr(double pot_chips, double effective_stack_chips);

/// Minimum of stacks; empty input returns `0`.
[[nodiscard]] double effective_stack(const std::vector<double>& stacks);

[[nodiscard]] double breakeven_call_equity(double pot_before_call, double to_call);

[[nodiscard]] double minimum_defense_frequency(double pot_before_opponent_bet,
                                               double opponent_bet_size);

[[nodiscard]] double stack_in_big_blinds(double stack_chips, double big_blind);

/// Pot odds as `pot_before_call : to_call` (e.g. `3.5` means 3.5:1).
[[nodiscard]] double pot_odds_ratio_display(double pot_before_call, double to_call);

[[nodiscard]] std::string format_pot_odds(double pot_before_call, double to_call,
                                            int decimals = 2);

[[nodiscard]] double rule_of_four_equity(double outs);

[[nodiscard]] double rule_of_two_equity(double outs);

/**
 * Implied-odds breakeven: average extra future win (beyond current pot + call) so a call is
 * neutral. Returns +infinity if `equity <= 0`.
 */
[[nodiscard]] double implied_breakeven_future_win(double pot_before_call, double to_call,
                                                  double equity);

/// Polarized river ratio `bet / (pot + 2*bet)` before hero's bet.
[[nodiscard]] double bluff_to_value_ratio(double pot_before_bet, double bet_size);

/// `1 / bluff_to_value_ratio`; returns +infinity when bluff ratio is `0`.
[[nodiscard]] double value_to_bluff_ratio(double pot_before_bet, double bet_size);

// --- Sizing & commitment ---

/// `bet_size / pot_before_bet`. If `pot_before_bet == 0` and `bet_size > 0`, returns +infinity.
[[nodiscard]] double bet_as_pot_fraction(double pot_before_bet, double bet_size);

/**
 * SPR after hero calls: remaining stack divided by new pot.
 * Assumes heads-up single call: new pot = `pot_before_call + 2 * to_call`.
 */
[[nodiscard]] double spr_after_call(double pot_before_call, double to_call,
                                    double effective_stack_before_call);

/// `to_call / effective_stack_before_call`; `0` if stack is `0`.
[[nodiscard]] double commitment_ratio(double to_call, double effective_stack_before_call);

// --- Game theory (alpha complements MDF) ---

/// `1 - MDF` = `bet / (pot + bet)` — exploit weight if hero never defends.
[[nodiscard]] double alpha_frequency(double pot_before_bet, double bet_size);

// --- Fold equity ---

/**
 * Break-even fold frequency when equity if called is `0` and folding wins `pot_before`.
 * `FE = hero_bet / (pot_before + hero_bet)`.
 */
[[nodiscard]] double breakeven_fold_equity_pure_bluff(double pot_before_hero_bet,
                                                      double hero_bet_or_call_size);

/**
 * Two-outcome model: `EV = FE * pot_before + (1-FE) * (equity * total_pot_if_called -
 * hero_bet_size)`. Returns required `FE` in `[0,1]` when the solution lies in that interval;
 * may exceed `1` if the line is -EV even if villain always folds (caller interprets).
 * Returns `0` when `equity * total_pot_if_called >= hero_bet_size` (no FE needed).
 */
[[nodiscard]] double breakeven_fold_equity_semi_bluff(double pot_before_hero_bet,
                                                      double hero_bet_size,
                                                      double equity_when_called,
                                                      double total_pot_if_called);

// --- P1 / draw probability ---

/// One card from `unseen_cards` without replacement; `outs` clean successes. P = outs/unseen_cards.
[[nodiscard]] double hypergeometric_one_card_hit_probability(double outs, double unseen_cards);

/// P(both next two cards are in suit) = C(s,2)/C(u,2); backdoor flush completes on turn+river.
[[nodiscard]] double runner_runner_flush_two_card_probability(double suit_cards_remaining,
                                                              double unseen_cards);

/// Two streets, flop to river: P(at least one hit) with disjoint out count `outs`.
[[nodiscard]] double flop_to_river_at_least_one_hit_probability(double outs, double unseen_after_flop);

/// P6: max extra chips lost on future streets when losing, keeping current call EV >= 0 (toy model).
[[nodiscard]] double reverse_implied_odds_max_future_loss(double pot_before_call, double to_call,
                                                         double equity);

/// P7: pot after `n_rounds` of matched pot-fraction `fraction` bets (both players), starting `pot0`.
[[nodiscard]] double geometric_pot_after_matched_pot_fractions(double pot0, double fraction,
                                                                int n_rounds);

/// P11: Harrington M = stack / (sb + bb + antes).
[[nodiscard]] double harrington_m(double stack_chips, double small_blind, double big_blind,
                                  double total_antes);

/// P12: full Kelly fraction for binary bet: win net `net_odds` per unit staked, win prob `p`.
[[nodiscard]] double kelly_criterion_binary(double win_probability, double net_odds);

/// P15: binomial SE for MC proportion estimate.
[[nodiscard]] double monte_carlo_standard_error(double p_hat, int n_trials);

struct Beta_binomial_fold_posterior {
    double alpha{};
    double beta{};
    double posterior_mean{};
};

/// P24: Beta(prior) on fold rate after observing folds/calls.
[[nodiscard]] Beta_binomial_fold_posterior beta_binomial_fold_update(double prior_alpha,
                                                                     double prior_beta, int folds,
                                                                     int calls);

/// P25: heuristic down-weighting of outs with multiple villains.
[[nodiscard]] double duplication_adjusted_outs(double outs, int num_villains,
                                                 double duplication_weight);

// --- P13 / P14 risk ---

/// Diffusion-style risk of ruin approximation in (0,1]; requires drift > 0.
[[nodiscard]] double risk_of_ruin_diffusion_approx(double drift_per_hand, double variance_per_hand,
                                                     double bankroll);

/// Inverse of `risk_of_ruin_diffusion_approx` for bankroll given target ROR.
[[nodiscard]] double bankroll_for_target_ror_diffusion(double drift_per_hand,
                                                       double variance_per_hand,
                                                       double target_ror);

struct Wilson_interval {
    double lower{};
    double upper{};
};

/// P16: Wilson score interval for binomial proportion; `z` e.g. 1.96 for ~95%.
[[nodiscard]] Wilson_interval wilson_score_interval(int successes, int n_trials, double z);

// --- P9 / P10 rake (winner-takes pot after rake from final pot) ---

[[nodiscard]] double rake_from_pot(double pot_chips, double rake_fraction, double rake_cap);

[[nodiscard]] double breakeven_call_equity_with_rake(double pot_before_call, double to_call,
                                                     double rake_fraction, double rake_cap);

[[nodiscard]] double breakeven_fold_equity_semi_bluff_with_rake(double pot_before_hero_bet,
                                                                double hero_bet_size,
                                                                double equity_when_called,
                                                                double total_pot_if_called,
                                                                double rake_fraction,
                                                                double rake_cap);

// --- P5 / P8 ---

/// P5: symmetric callers: `k` opponents each match `to_call` after hero calls.
[[nodiscard]] double multiway_symmetric_breakeven_call_equity(double pot_before, double to_call,
                                                              int symmetric_extra_callers);

/**
 * P8: same fold equity `fe` on two streets; pure air; pot P0, bets B1 then B2 into P0+2*B1.
 * Returns `fe` in [0,1] with EV=0, or NaN if no solution in [0,1].
 */
[[nodiscard]] double two_street_pure_bluff_same_fold_equity(double pot_before_street1,
                                                             double bet_street1, double bet_street2);

// --- P4: disjoint out groups (union bound exact for two draws when groups disjoint on cards) ---

[[nodiscard]] double flop_to_river_at_least_one_hit_disjoint_outs_sum(double unseen_after_flop,
                                                                      const std::vector<double>& outs_per_disjoint_category);

/**
 * P23: symmetric jam `S` each, pot `2S + deadMoney` if called; breakeven solves
 * `equity * (2S + dead) - S = 0` => `S = equity*dead/(1-2*equity)` for `equity < 0.5`.
 * Returns `+Infinity` when `equity > 0.5` (toy model: jam arbitrarily large is +EV).
 */
[[nodiscard]] double chubukov_symmetric_jam_breakeven_stack(double dead_money_chips, double equity);

}  // namespace poker
