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

}  // namespace poker
