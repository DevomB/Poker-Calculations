#pragma once

#include <cstddef>
#include <vector>

namespace poker {

/**
 * Covering PKO knockout model.
 *
 * P(j busts) = Harville last-place of j among players with chips
 * (`icm_last_place_probabilities_harville` on the alive subset).
 * P(i knocks j | j busts) = stack_i / (totalChips - stack_j) when i covers j
 * (`stack_i >= stack_j` and i != j), else 0.
 * Diagonal is 0. Rows need not sum to 1 (multiple KOs; non-coverers get 0 without
 * renormalizing). Zero-stack players are already out: they neither collect nor
 * are collected. n in 2..31 (Harville limit).
 */
struct PkoKnockoutMatrix {
    std::vector<double> flat;  // n×n row-major; entry i*n+j = P(i collects j's bounty)
    std::size_t n = 0;
};

[[nodiscard]] PkoKnockoutMatrix pko_knockout_probability_matrix(const std::vector<double>& stacks);

/// E[bounty $] per player: sum_j bounty_value[j] * P(i knocks j). Self-bounty ignored (diag 0).
[[nodiscard]] std::vector<double> pko_expected_bounty_collection(const std::vector<double>& stacks,
                                                                 const std::vector<double>& bounty_values);

struct PkoIcmbuResult {
    std::vector<double> icm;
    std::vector<double> bounty;
    std::vector<double> icmbu;  // icm + bounty
};

/// ICMBU: freezeout `icm_expected_payouts` plus expected bounty collection.
[[nodiscard]] PkoIcmbuResult pko_icmbu_payouts(const std::vector<double>& stacks,
                                               const std::vector<double>& payouts,
                                               const std::vector<double>& bounty_values);

struct PkoBountyRiskPremiumResult {
    std::vector<double> freezeout_icm;
    std::vector<double> icmbu;
    std::vector<double> icmbu_minus_freezeout;  // = expected bounty collection
    std::vector<double> chip_share_bounty_ev;   // (stack_i / total) * bounty pool
    std::vector<double> bounty_risk_premium;    // chip share − expected collection
};

/// Freezeout ICM vs ICMBU, and chip-share of the bounty pool vs expected collection.
[[nodiscard]] PkoBountyRiskPremiumResult pko_bounty_risk_premium(const std::vector<double>& stacks,
                                                                 const std::vector<double>& payouts,
                                                                 const std::vector<double>& bounty_values);

struct PkoSpotEvResult {
    double take_ev = 0.0;  // call or jam
    double fold_ev = 0.0;
    double delta = 0.0;  // take − fold
};

/**
 * $EV(call all-in) vs $EV(fold) when villain has a bounty.
 *
 * `stacks` are remaining chips (pot already posted). Fold leaves stacks as given:
 * ICM + future bounty EV on those stacks.
 * Call: effective = min(hero, villain); pot_final = pot + 2*effective.
 * Win: hero takes pot_final, collects villain bounty if villain is covered and busts.
 * Lose: villain takes pot_final; hero may bust. Tie: pot split, no KO.
 * Post-hand $EV = ICM allowing busts + immediate KO bounty + future bounty EV.
 */
[[nodiscard]] PkoSpotEvResult pko_call_ev_vs_shove(const std::vector<double>& stacks,
                                                   const std::vector<double>& payouts,
                                                   const std::vector<double>& bounty_values,
                                                   std::size_t hero, std::size_t villain, double pot,
                                                   double p_win, double p_tie, double p_lose);

/**
 * $EV(jam) vs $EV(fold), including bounties.
 * Villain folds with `fold_equity`: hero is assigned `pot` chips (no KO).
 * Called: same all-in resolution as `pko_call_ev_vs_shove` with `equity_when_called`.
 * Hero fold: stacks unchanged (pot already committed).
 */
[[nodiscard]] PkoSpotEvResult pko_jam_ev_vs_fold(const std::vector<double>& stacks,
                                                 const std::vector<double>& payouts,
                                                 const std::vector<double>& bounty_values,
                                                 std::size_t hero, std::size_t villain, double pot,
                                                 double fold_equity, double p_win, double p_tie,
                                                 double p_lose);

struct MysteryBountyEvResult {
    double one_draw = 0.0;       // weighted mean of remaining prizes
    double all_remaining = 0.0;  // sum (winner-take-all leftover pool)
    double sample_k = 0.0;       // E[sum of k WOR draws]; equal-weight exact via linearity
    int k = 0;
};

/// Mystery bounty EV. `weights` empty ⇒ equal. `k==0` ⇒ sample_k unused (still one_draw + all).
[[nodiscard]] MysteryBountyEvResult mystery_bounty_expected_value(const std::vector<double>& values,
                                                                  const std::vector<double>& weights,
                                                                  int k);

/**
 * Progressive KO posted bounty: posted_i = base_i + carry * collected_i.
 * `knockouts` length n: dollars already collected by i (onto their head).
 * `knockouts` length n*n: row-major M, collected_i = sum_j M[i,j] * base_j.
 * carry 1.0 = classic PKO (you carry what they posted).
 */
[[nodiscard]] std::vector<double> progressive_ko_posted_bounty(const std::vector<double>& base_bounties,
                                                               const std::vector<double>& knockouts,
                                                               double carry_fraction);

struct PkoCoveringHuntResult {
    double hunt_ev = 0.0;
    double check_down_ev = 0.0;
    double delta = 0.0;
    double equity_used = 0.0;
};

/**
 * Isolate vs a covered shorter stack (HU all-in, rest fold).
 * Card equity default: stack-ratio proxy hunter / (hunter + prey).
 * Hunt uses the call-all-in model; check-down is fold (stacks unchanged).
 */
[[nodiscard]] PkoCoveringHuntResult pko_covering_hunt_ev(const std::vector<double>& stacks,
                                                         const std::vector<double>& payouts,
                                                         const std::vector<double>& bounty_values,
                                                         std::size_t hunter, std::size_t prey, double pot,
                                                         double equity, bool equity_provided);

struct PkoWinnerTakeBountiesResult {
    std::vector<double> adjusted_payouts;  // first prize += remaining bounty pool
    std::vector<double> ev;                // ICM on adjusted payouts
    std::vector<double> win_probabilities;
    std::vector<double> bounty_to_winner_ev;  // P(i first) * remaining pool
};

/// Final-table leftover bounty pool awarded to the winner (added to first prize).
[[nodiscard]] PkoWinnerTakeBountiesResult pko_winner_take_remaining_bounties(
    const std::vector<double>& stacks, const std::vector<double>& payouts, double remaining_bounty_pool);

}  // namespace poker
