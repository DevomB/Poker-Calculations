#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace poker {

/**
 * Average-position future-game simulation $EV.
 *
 * Each orbit every still-alive seat pays `min(stack, sb+bb+ante)` via `orbit_cost_chips`.
 * Paid chips leave the stacks (dead pool) — they are **not** given to a button/blind seat.
 * Bust at 0. Busted seats get $0; remaining seats take Harville ICM on the **top-k** prizes.
 * `orbits == 0` or a zero orbit cost leaves stacks unchanged (equals `icm_expected_payouts`
 * when every stack is still positive).
 */
[[nodiscard]] std::vector<double> future_game_simulation_payouts(const std::vector<double>& stacks,
                                                                 const std::vector<double>& payouts,
                                                                 int orbits, double small_blind,
                                                                 double big_blind, double ante);

struct FutureGrowthShareResult {
    std::vector<double> net_growth;
    std::vector<double> growth_share;
    std::size_t survivor_count{0};
};

/**
 * Average-position blind growth: each alive seat pays the orbit cost; after busts, the
 * collected pool is split equally among survivors (short stacks stop paying). Equal stacks
 * that all survive have pay == receive and zero net. `growth_share` is each survivor's
 * fraction of chips received (0 for busted seats).
 */
[[nodiscard]] FutureGrowthShareResult future_growth_share(const std::vector<double>& stacks, int orbits,
                                                          double small_blind, double big_blind,
                                                          double ante);

/**
 * Subtract posted blinds/antes, then ICM on remaining stacks. Hero posts `hero_post`;
 * every other seat posts `posts[i]`. Pot is dead for placement ICM (standard).
 */
[[nodiscard]] std::vector<double> icm_payouts_after_blind_post(const std::vector<double>& stacks,
                                                               const std::vector<double>& payouts,
                                                               std::size_t hero, double hero_post,
                                                               const std::vector<double>& posts);

struct IcmDecisionEv {
    double fold_ev{0.0};
    double take_ev{0.0};
    double delta{0.0};
};

/**
 * $EV(fold) = ICM on the given stacks (blinds already posted if the caller modeled that).
 * $EV(jam) = foldEquity * ICM(hero collects `pot`, villain loses nothing extra)
 *          + (1-foldEquity) * mix of ICM after a stack-off resolved with `equity_when_called`
 *            (win share of the pot, ties as half).
 */
[[nodiscard]] IcmDecisionEv icm_jam_vs_fold_ev(const std::vector<double>& stacks,
                                               const std::vector<double>& payouts, std::size_t hero,
                                               std::size_t villain, double pot, double fold_equity,
                                               double equity_when_called);

/**
 * Facing a shove. Fold keeps stacks as given. Call puts `call_amount` from hero and
 * `min(villain, call)` from villain into `pot`, then mixes win/lose ICM by `hero_equity`.
 */
[[nodiscard]] IcmDecisionEv icm_call_vs_fold_ev(const std::vector<double>& stacks,
                                                const std::vector<double>& payouts, std::size_t hero,
                                                std::size_t villain, double pot, double call_amount,
                                                double hero_equity);

/**
 * Calling bubble factor for one hero-vs-villain all-in:
 * BF = (EV_now - EV_lose) / (EV_win - EV_now) after transferring `chips_at_risk`.
 * Distinct from `icm_pairwise_bubble_factor`. Tiny gain → +∞ when there is a real dollar loss.
 */
[[nodiscard]] double icm_calling_bubble_factor(const std::vector<double>& stacks,
                                               const std::vector<double>& payouts, std::size_t hero,
                                               std::size_t villain, double chips_at_risk);

/// FGS over a blind schedule. Each level applies `orbits_at_level[i]` average-position orbits.
[[nodiscard]] std::vector<double> fgs_payouts_blind_schedule(const std::vector<double>& stacks,
                                                             const std::vector<double>& payouts,
                                                             const std::vector<double>& small_blinds,
                                                             const std::vector<double>& big_blinds,
                                                             const std::vector<double>& antes,
                                                             const std::vector<double>& orbits_at_level);

struct IcmStallingEvResult {
    double now_ev{0.0};
    double stall_ev{0.0};
    double stalling_premium{0.0};
    double collision_ev{0.0};
    bool collision_modeled{false};
};

/**
 * Stalling premium = FGS(1 orbit)[hero] − ICM now[hero].
 * Optional collision: two shortest alive stacks go all-in 50/50 for `min` chips (no extra pot).
 */
[[nodiscard]] IcmStallingEvResult icm_stalling_ev(const std::vector<double>& stacks,
                                                  const std::vector<double>& payouts, std::size_t hero,
                                                  double small_blind, double big_blind, double ante,
                                                  bool model_collision);

struct IcmPayJumpSurvivalResult {
    double now_ev{0.0};
    double after_bust_ev{0.0};
    double ladder_delta{0.0};
    std::size_t busted_index{0};
};

/**
 * $EV if the shortest *other* alive stack busts next.
 * `vanish`: their chips leave the table. `chipLeader`: chips move to the current chip leader.
 */
[[nodiscard]] IcmPayJumpSurvivalResult icm_pay_jump_survival_ev(const std::vector<double>& stacks,
                                                                const std::vector<double>& payouts,
                                                                std::size_t hero,
                                                                const std::string& bust_chips);

struct IcmDeadPotDollarEvResult {
    double now_ev{0.0};
    double win_ev{0.0};
    double delta{0.0};
};

/**
 * $EV of hero winning a dead pot of `dead_chips` (chips already in the middle, owned by nobody).
 * Placement is Harville on stacks; winning the pot is a two-point ICM: hero stack += dead_chips.
 */
[[nodiscard]] IcmDeadPotDollarEvResult icm_dead_pot_dollar_ev(const std::vector<double>& stacks,
                                                              const std::vector<double>& payouts,
                                                              std::size_t hero, double dead_chips);

}  // namespace poker
