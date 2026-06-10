#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace poker {

/// structured straight-draw runner patterns (distinct straight-completing unseen cards).
enum class Runner_runner_straight_draw_kind {
    GutshotFourOut = 0,
    OpenEndedEightOut = 1,
    DoubleBellyBusterEightOut = 2,
};

/// how hero shares the pot when winning after calling (symmetric multiway toy).
enum class Multiway_symmetric_pot_share_model {
    WinnerTakesAll = 0,
    /** Hero wins `hero_fraction_of_pot_when_win` of the **final** pot when holding the best hand. */
    FixedHeroShareWhenWins = 1,
};

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

// --- draw probability ---

/// One card from `unseen_cards` without replacement; `outs` clean successes. P = outs/unseen_cards.
[[nodiscard]] double hypergeometric_one_card_hit_probability(double outs, double unseen_cards);

/// P(both next two cards are in suit) = C(s,2)/C(u,2); backdoor flush completes on turn+river.
[[nodiscard]] double runner_runner_flush_two_card_probability(double suit_cards_remaining,
                                                              double unseen_cards);

/// Two streets, flop to river: P(at least one hit) with one clean out count `outs` (two draws).
[[nodiscard]] double flop_to_river_at_least_one_hit_probability(double outs, double unseen_after_flop);

/**
 * two overlapping out categories (card counts). Union = `outs_a + outs_b - overlap_ab`; then the
 * standard two-draw “at least one” formula.
 */
[[nodiscard]] double flop_to_river_two_category_union_hit_probability(double unseen_after_flop,
                                                                      double outs_a, double outs_b,
                                                                      double overlap_ab);

/** same as `flop_to_river_two_category_union_hit_probability` (shared_ab = overlap_ab). */
[[nodiscard]] double flop_to_river_at_least_one_hit_union_two_categories(double unseen_after_flop,
                                                                         double outs_a, double outs_b,
                                                                         double shared_ab);

/**
 * three categories with pairwise and triple intersection sizes (card counts). Union size =
 * `oa+ob+oc - sab - sac - sbc + sabc`.
 */
[[nodiscard]] double flop_to_river_at_least_one_hit_union_three_categories(
    double unseen_after_flop, double outs_a, double outs_b, double outs_c, double shared_ab,
    double shared_ac, double shared_bc, double shared_abc);

/**
 * four categories; intersections are **card counts**. Pair order (0,1)(0,2)(0,3)(1,2)(1,3)(2,3);
 * triple order (0,1,2)(0,1,3)(0,2,3)(1,2,3); `four_way` = |A∩B∩C∩D|.
 */
[[nodiscard]] double flop_to_river_at_least_one_hit_union_four_categories(
    double unseen_after_flop, double oa, double ob, double oc, double od, double s01, double s02,
    double s03, double s12, double s13, double s23, double s012, double s013, double s023, double s123,
    double four_way);

/**
 * canonical 4- or 8-out **straight-draw** runner model (subtract dead cards from pattern size).
 * For **exact** straight-or-better rate from concrete flop cards, use
 * `straight_made_flop_to_river_exact_probability` in `exact_equity.hpp`.
 */
[[nodiscard]] double runner_runner_straight_draw_hit_probability(
    Runner_runner_straight_draw_kind kind, int dead_cards_among_pattern_outs, double unseen_after_flop);

/// max extra chips lost on future streets when losing, keeping current call EV >= 0 (toy model).
[[nodiscard]] double reverse_implied_odds_max_future_loss(double pot_before_call, double to_call,
                                                         double equity);

/// pot after `n_rounds` of matched pot-fraction `fraction` bets (both players), starting `pot0`.
[[nodiscard]] double geometric_pot_after_matched_pot_fractions(double pot0, double fraction,
                                                                int n_rounds);

/// Harrington M = stack / (sb + bb + total_antes) with caller-supplied total antes.
[[nodiscard]] double harrington_m(double stack_chips, double small_blind, double big_blind,
                                  double total_antes);

/**
 * Effective Harrington M: `stack / (sb + bb + ante_per_active_player * num_active_players)` — antes only
 * from `num_active_players` seats each paying `ante_per_active_player`.
 */
[[nodiscard]] double harrington_m_effective(double stack_chips, double small_blind, double big_blind,
                                          double ante_per_active_player, int num_active_players);

/**
 * effective M when antes differ by seat — pass one entry per **active** seat (zeros allowed);
 * `total_antes` = sum of entries.
 */
[[nodiscard]] double harrington_m_effective_active_antes(double stack_chips, double small_blind,
                                                        double big_blind,
                                                        const std::vector<double>& antes_from_active_seats);

/// full Kelly fraction for binary bet: win net `net_odds` per unit staked, win prob `p`.
[[nodiscard]] double kelly_criterion_binary(double win_probability, double net_odds);

/// binomial SE for MC proportion estimate.
[[nodiscard]] double monte_carlo_standard_error(double p_hat, int n_trials);

/**
 * Smallest integer trial count so binomial SE (at `p_hat`) is at most `target_se` (ceil of `p(1-p)/se²`).
 * Throws if `p_hat` is not in `(0,1)` or `target_se` is not finite and positive.
 */
[[nodiscard]] std::int64_t monte_carlo_trials_for_standard_error_bound(double p_hat, double target_se);

/**
 * Uncapped algebraic inverse of the rule-of-two heuristic: `outs ≈ equity * unseen / 2`, clamped to
 * `[0, unseen]` — not an exact inverse of `rule_ofTwoEquity` (which caps outs at 48 and equity at 1).
 */
[[nodiscard]] double estimated_outs_from_rule_of_two(double equity, double unseen_cards);

/// Same idea for two streets vs `ruleOfFourEquity`: `outs ≈ equity * unseen / 4`, clamped to `[0, unseen]`.
[[nodiscard]] double estimated_outs_from_rule_of_four(double equity, double unseen_cards);

/**
 * Chip EV of calling once vs folding (0), heads-up single call, when the **final** pot pays rake like
 * `breakevenCallEquityWithRake`. `pot_before_call` is chips in the pot before hero's call.
 */
[[nodiscard]] double expected_value_call_with_rake(double equity, double pot_before_call, double to_call,
                                                   double rake_fraction, double rake_cap);

/// NL toy: minimum **total** wager after a raise = `current_max_wager + max(last_raise_increment, big_blind)`.
[[nodiscard]] double nl_minimum_raise_to_total(double current_max_wager, double last_raise_increment,
                                               double big_blind);

/// One full orbit: `small_blind + big_blind + sum(antes_from_seats)`.
[[nodiscard]] double orbit_cost_chips(double small_blind, double big_blind,
                                     const std::vector<double>& antes_from_seats);

/// Harrington Q: `hero_stack / mean(stacks)` (pressure vs table average); all stacks must be positive.
[[nodiscard]] double harrington_q(double hero_stack, const std::vector<double>& stacks);

/// NLHE preflop combo count from shorthand (`AA`, `AKs`, `AKo`); throws on invalid notation.
[[nodiscard]] int preflop_combos_from_notation(const std::string& notation);

struct Beta_binomial_fold_posterior {
    double alpha{};
    double beta{};
    double posterior_mean{};
};

/// Beta(prior) on fold rate after observing folds/calls.
[[nodiscard]] Beta_binomial_fold_posterior beta_binomial_fold_update(double prior_alpha,
                                                                     double prior_beta, int folds,
                                                                     int calls);

/// heuristic down-weighting of outs with multiple villains.
[[nodiscard]] double duplication_adjusted_outs(double outs, int num_villains,
                                                 double duplication_weight);

// --- risk of ruin ---

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

/// Wilson score interval for binomial proportion; `z` e.g. 1.96 for ~95%.
[[nodiscard]] Wilson_interval wilson_score_interval(int successes, int n_trials, double z);

/// Agresti–Coull interval for a binomial proportion (adds `z^2/2` pseudo-counts); `z` e.g. 1.96.
[[nodiscard]] Wilson_interval agresti_coull_interval(int successes, int n_trials, double z);

/**
 * Normal (Wald) interval `p_hat ± z * sqrt(p_hat(1-p_hat)/n)` clamped to `[0,1]`.
 * Weak when `p_hat` is near 0 or 1 with small `n` (can hit boundaries or collapse).
 */
[[nodiscard]] Wilson_interval normal_wald_binomial_interval(int successes, int n_trials, double z);

/**
 * Hoeffding: smallest integer `n` with `n >= ln(2/delta) / (2*epsilon^2)` so that with probability at
 * least `1-delta`, `|p_hat - p| <= epsilon` for any underlying `p` (MC proportion; independent trials).
 */
[[nodiscard]] std::int64_t monte_carlo_trials_for_hoeffding_bound(double epsilon, double delta);

/**
 * Breakeven call equity from `potOddsRatioDisplay` ratio `R = pot_before_call / to_call`:
 * `1 / (1 + R)` when `R` is finite; `0` when `R` is `+infinity` (no call to match).
 */
[[nodiscard]] double breakeven_call_equity_from_pot_odds_display_ratio(double display_pot_to_call_ratio);

/**
 * Inverse of `breakeven_call_equity_from_pot_odds_display_ratio`: `(1 - e) / e` for `e` in `(0,1]`;
 * `+infinity` when `e == 0`; `0` when `e == 1`.
 */
[[nodiscard]] double pot_odds_display_ratio_from_breakeven_call_equity(double breakeven_equity);

/// Each stack divided by the sum of stacks (not Harville ICM); all stacks finite and non-negative, sum positive.
[[nodiscard]] std::vector<double> normalized_stack_fractions(const std::vector<double>& stacks);

/// Sum of `preflop_combos_from_notation` over `notations` (empty list → `0`).
[[nodiscard]] int preflop_combos_from_notations_list(const std::vector<std::string>& notations);

/**
 * Reduced integer ratio string `pot : to_call` (e.g. `100` and `50` → `"2:1"`). Uses `std::gcd` on
 * rounded chip integers; `to_call == 0` → same infinity token as `format_pot_odds`.
 */
[[nodiscard]] std::string format_pot_odds_reduced_fraction(double pot_before_call, double to_call);

/**
 * Integer order `0..9` for `evaluateHandCategory` labels (`highCard` … `royalFlush`); throws if unknown.
 */
[[nodiscard]] int hand_rank_category_order(const std::string& category_camel_case);

/// Book-style winning odds-against: `(1 - equity) / equity` for `equity` in `(0,1]`; `+infinity` at `0`.
[[nodiscard]] double equity_to_winning_odds_against(double equity);

/// Inverse: `1 / (1 + odds_against)` for finite `odds_against >= 0`; `0` if `odds_against` is `+infinity`.
[[nodiscard]] double winning_odds_against_to_equity(double odds_against);

// --- rake-adjusted breakeven (winner-takes pot after rake from final pot) ---

[[nodiscard]] double rake_from_pot(double pot_chips, double rake_fraction, double rake_cap);

[[nodiscard]] double breakeven_call_equity_with_rake(double pot_before_call, double to_call,
                                                     double rake_fraction, double rake_cap);

[[nodiscard]] double breakeven_fold_equity_semi_bluff_with_rake(double pot_before_hero_bet,
                                                                double hero_bet_size,
                                                                double equity_when_called,
                                                                double total_pot_if_called,
                                                                double rake_fraction,
                                                                double rake_cap);

/**
 * pure-bluff breakeven FE when a fold wins `pot_before + hero_bet - rake` (rake on chips
 * shipped after villain folds).
 */
[[nodiscard]] double breakeven_fold_equity_pure_bluff_with_rake(double pot_before_hero_bet,
                                                                double hero_bet_or_call_size,
                                                                double rake_fraction, double rake_cap);

// --- two-street pure bluff ---

/// symmetric callers: `k` opponents each match `to_call` after hero calls.
[[nodiscard]] double multiway_symmetric_breakeven_call_equity(double pot_before, double to_call,
                                                              int symmetric_extra_callers);

/**
 * same geometry as `multiway_symmetric_breakeven_call_equity`, but hero receives only
 * `hero_fraction_of_pot_when_win` of the final pot when winning (e.g. 1/(1+t) for t-way chop proxy).
 * `model` must be `FixedHeroShareWhenWins` with fraction in (0,1], or `WinnerTakesAll` (fraction ignored).
 */
[[nodiscard]] double multiway_symmetric_breakeven_call_equity_with_share(
    double pot_before, double to_call, int symmetric_extra_callers,
    Multiway_symmetric_pot_share_model model, double hero_fraction_of_pot_when_win);

/**
 * same fold equity `fe` on two streets; pure air; pot P0, bets B1 then B2 into P0+2*B1.
 * Returns `fe` in [0,1] with EV=0, or NaN if no solution in [0,1].
 */
[[nodiscard]] double two_street_pure_bluff_same_fold_equity(double pot_before_street1,
                                                             double bet_street1, double bet_street2);

/// EV of two-street pure-bluff line given independent fold rates `fe1`, `fe2` (linear accounting).
[[nodiscard]] double two_street_pure_bluff_ev(double pot_before_street1, double bet_street1,
                                              double bet_street2, double fold_equity_street1,
                                              double fold_equity_street2);

/**
 * given `fold_equity_street1`, returns breakeven `fold_equity_street2` for pure air (may lie outside
 * [0,1]). Throws if `fold_equity_street1` is 1 (degenerate).
 */
[[nodiscard]] double breakeven_fold_equity_second_street_pure_bluff(double pot_before_street1,
                                                                      double bet_street1, double bet_street2,
                                                                      double fold_equity_street1);

/**
 * given `fold_equity_street2`, returns breakeven `fold_equity_street1` for pure air (may lie outside
 * [0,1]). Throws if `fold_equity_street2` is 1 (degenerate).
 */
[[nodiscard]] double breakeven_fold_equity_first_street_pure_bluff(double pot_before_street1,
                                                                   double bet_street1, double bet_street2,
                                                                   double fold_equity_street2);

// --- disjoint out categories (caller must ensure categories do not share outs) ---

[[nodiscard]] double flop_to_river_at_least_one_hit_disjoint_outs_sum(double unseen_after_flop,
                                                                      const std::vector<double>& outs_per_disjoint_category);

/**
 * symmetric jam `S` each, pot `2S + deadMoney` if called; breakeven solves
 * `equity * (2S + dead) - S = 0` => `S = equity*dead/(1-2*equity)` for `equity < 0.5`.
 * Returns `+Infinity` when `equity > 0.5` (toy model: jam arbitrarily large is +EV).
 */
[[nodiscard]] double chubukov_symmetric_jam_breakeven_stack(double dead_money_chips, double equity);

/// EV of symmetric jam `S` chips each when called: `equity * (2S + dead) - S`.
[[nodiscard]] double chubukov_symmetric_jam_ev(double jam_stack_chips, double dead_money_chips,
                                               double equity);

/**
 * largest integer jam stack in `[1, max_stack_chips]` with nonnegative symmetric-jam EV
 * (binary search). When `equity > 0.5`, returns `max_stack_chips`.
 */
[[nodiscard]] int chubukov_max_symmetric_jam_stack_chips_binary_search(double equity,
                                                                       double dead_money_chips,
                                                                       int max_stack_chips);

// --- single-street draws ---
[[nodiscard]] double one_street_at_least_one_hit_probability(double outs, double unseen);
[[nodiscard]] double flop_to_turn_at_least_one_hit_probability(double outs, double unseen_after_flop);
[[nodiscard]] double turn_to_river_at_least_one_hit_probability(double outs, double unseen_after_turn);
[[nodiscard]] double flop_to_turn_at_least_one_hit_union_two_categories(double unseen, double outs_a,
                                                                        double outs_b, double shared_ab);
[[nodiscard]] double turn_to_river_at_least_one_hit_union_two_categories(double unseen, double outs_a,
                                                                         double outs_b, double shared_ab);
[[nodiscard]] double flop_to_turn_at_least_one_hit_union_three_categories(
    double unseen, double outs_a, double outs_b, double outs_c, double shared_ab, double shared_ac,
    double shared_bc, double shared_abc);
[[nodiscard]] double turn_to_river_at_least_one_hit_union_three_categories(
    double unseen, double outs_a, double outs_b, double outs_c, double shared_ab, double shared_ac,
    double shared_bc, double shared_abc);
[[nodiscard]] double flop_to_turn_at_least_one_hit_union_four_categories(
    double unseen, double oa, double ob, double oc, double od, double s01, double s02, double s03,
    double s12, double s13, double s23, double s012, double s013, double s023, double s123,
    double four_way);
[[nodiscard]] double turn_to_river_at_least_one_hit_union_four_categories(
    double unseen, double oa, double ob, double oc, double od, double s01, double s02, double s03,
    double s12, double s13, double s23, double s012, double s013, double s023, double s123,
    double four_way);
[[nodiscard]] double flop_to_turn_at_least_one_hit_disjoint_outs_sum(
    double unseen, const std::vector<double>& outs_per_disjoint_category);
[[nodiscard]] double turn_to_river_at_least_one_hit_disjoint_outs_sum(
    double unseen, const std::vector<double>& outs_per_disjoint_category);
[[nodiscard]] double hypergeometric_two_card_hit_probability(double outs, double unseen_cards);
[[nodiscard]] double hypergeometric_two_card_miss_probability(double outs, double unseen_cards);
[[nodiscard]] double runner_runner_backdoor_flush_one_card_probability(double suit_cards_remaining,
                                                                       double unseen_cards);
[[nodiscard]] double blocker_adjusted_outs(double outs, double blocker_fraction);
[[nodiscard]] double suit_blocker_fraction(double suit_cards_dead, double unseen);

// --- pot / rake / raise ---
[[nodiscard]] double net_pot_after_rake(double pot_chips, double rake_fraction, double rake_cap);
[[nodiscard]] double net_pot_after_call_and_rake(double pot_before_call, double to_call,
                                                 double rake_fraction, double rake_cap);
[[nodiscard]] double effective_pot_odds_display_after_rake(double pot_before_call, double to_call,
                                                           double rake_fraction, double rake_cap);
[[nodiscard]] double implied_breakeven_total_pot(double pot_before_call, double to_call, double equity);
[[nodiscard]] double implied_odds_required_equity_from_future_win(double pot_before_call, double to_call,
                                                                  double future_win);
[[nodiscard]] double expected_value_raise(double equity_when_called, double pot_before_raise,
                                          double raise_size, double fold_equity, double pot_if_called);
[[nodiscard]] double expected_value_raise_with_rake(double equity_when_called, double pot_before_raise,
                                                    double raise_size, double fold_equity,
                                                    double pot_if_called, double rake_fraction,
                                                    double rake_cap);
[[nodiscard]] double breakeven_raise_equity(double pot_before_raise, double raise_size,
                                            double fold_equity, double pot_if_called);
[[nodiscard]] double breakeven_call_equity_with_posted_ante(double pot_before_call, double to_call,
                                                            double ante_to_post);
[[nodiscard]] double pot_size_after_hu_call(double pot_before_call, double to_call);
[[nodiscard]] double pot_size_after_hu_bet(double pot_before_bet, double bet_size);
[[nodiscard]] double expected_value_per_big_blind(double chip_ev, double big_blind);

// --- GTO with rake ---
[[nodiscard]] double minimum_defense_frequency_with_rake(double pot_before_bet, double bet_size,
                                                         double rake_fraction, double rake_cap);
[[nodiscard]] double alpha_frequency_with_rake(double pot_before_bet, double bet_size,
                                               double rake_fraction, double rake_cap);
[[nodiscard]] double bluff_to_value_ratio_with_rake(double pot_before_bet, double bet_size,
                                                    double rake_fraction, double rake_cap);
[[nodiscard]] double value_to_bluff_ratio_with_rake(double pot_before_bet, double bet_size,
                                                    double rake_fraction, double rake_cap);

// --- sizing ---
[[nodiscard]] double spr_after_bet(double pot_before_bet, double bet_size,
                                  double effective_stack_before_bet);
[[nodiscard]] double spr_after_raise(double pot_before_raise, double raise_size,
                                     double effective_stack_before_raise);
[[nodiscard]] double commitment_ratio_after_bet(double bet_size, double effective_stack_before_bet);
[[nodiscard]] double bet_size_to_match_pot_fraction(double pot_before_bet, double target_fraction);

// --- Kelly ---
[[nodiscard]] double half_kelly_criterion_binary(double win_probability, double net_odds);
[[nodiscard]] double quarter_kelly_criterion_binary(double win_probability, double net_odds);
[[nodiscard]] double eighth_kelly_criterion_binary(double win_probability, double net_odds);
[[nodiscard]] double kelly_criterion_binary_clamped(double win_probability, double net_odds);

// --- fold equity extensions ---
[[nodiscard]] double breakeven_fold_equity_pure_bluff_with_ante(double pot_before_hero_bet,
                                                                double hero_bet_or_call_size,
                                                                double ante_to_post);
[[nodiscard]] double breakeven_fold_equity_semi_bluff_with_ante(double pot_before_hero_bet,
                                                                double hero_bet_size,
                                                                double equity_when_called,
                                                                double total_pot_if_called,
                                                                double ante_to_post);
[[nodiscard]] double two_street_pure_bluff_ev_with_rake(double pot_before_street1, double bet_street1,
                                                        double bet_street2, double fold_equity_street1,
                                                        double fold_equity_street2, double rake_fraction,
                                                        double rake_cap);
[[nodiscard]] double three_street_pure_bluff_same_fold_equity(double pot_before_street1,
                                                              double bet_street1, double bet_street2,
                                                              double bet_street3);
[[nodiscard]] double three_street_pure_bluff_ev(double pot_before_street1, double bet_street1,
                                                double bet_street2, double bet_street3,
                                                double fold_equity_street1, double fold_equity_street2,
                                                double fold_equity_street3);

// --- multiway ---
[[nodiscard]] double multiway_symmetric_breakeven_call_equity_with_rake(
    double pot_before, double to_call, int symmetric_extra_callers, double rake_fraction,
    double rake_cap);
[[nodiscard]] double multiway_symmetric_breakeven_call_equity_with_share_and_rake(
    double pot_before, double to_call, int symmetric_extra_callers,
    Multiway_symmetric_pot_share_model model, double hero_fraction_of_pot_when_win,
    double rake_fraction, double rake_cap);
[[nodiscard]] double multiway_expected_value_call(double equity, double pot_before, double to_call,
                                                  int symmetric_extra_callers);

// --- reverse implied / geometry ---
[[nodiscard]] double reverse_implied_odds_min_equity(double pot_before_call, double to_call,
                                                     double max_future_loss);
[[nodiscard]] double geometric_pot_after_single_matched_bet(double pot0, double bet_size);

// --- statistics ---
[[nodiscard]] double binomial_proportion_ci_width(int successes, int n_trials, double z);
[[nodiscard]] std::int64_t monte_carlo_trials_for_wilson_half_width(double p_hat,
                                                                    double target_half_width, double z);
[[nodiscard]] double variance_to_standard_deviation_per_hand(double variance_per_hand);

// --- stacks ---
[[nodiscard]] int preflop_combos_from_notation_minus_blockers(const std::string& notation,
                                                              int dead_cards_among_combos);
[[nodiscard]] double stack_to_pot_after_call(double pot_before_call, double to_call,
                                             double effective_stack_before_call);

// --- push/fold toys ---
[[nodiscard]] double push_fold_symmetric_ev(double equity, double jam_stack_chips,
                                            double dead_money_chips);
[[nodiscard]] double push_fold_symmetric_breakeven_equity(double jam_stack_chips,
                                                          double dead_money_chips);
[[nodiscard]] double open_raise_breakeven_fold_equity(double pot_before_hero_bet,
                                                    double hero_open_raise_size);
[[nodiscard]] double call_or_fold_chip_ev_delta(double equity, double pot, double to_call);

// --- exact made hands (shared enumerator) ---
[[nodiscard]] double made_category_flop_to_river_exact_probability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards,
    const std::function<bool(HandRank)>& category_hits,
    const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double flush_made_flop_to_river_exact_probability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* should_cancel = nullptr);
[[nodiscard]] double full_house_made_flop_to_river_exact_probability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* should_cancel = nullptr);
[[nodiscard]] double trips_made_flop_to_river_exact_probability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* should_cancel = nullptr);
[[nodiscard]] double two_pair_made_flop_to_river_exact_probability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double exact_hero_category_at_least_flop_to_river(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three_cards,
    const std::vector<Card>& known_dead_cards, int min_category_order,
    const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double normalized_range_weight_sum(const std::vector<double>& weights);

}  // namespace poker
