#pragma once

#include "poker/nash_push_fold.hpp"
#include "poker/pko.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace poker {

/**
 * 3-max Spin & Go prize vector: `multiplier * buyin`.
 * Default split 50/30/20. `winner_take_all` → 100/0/0.
 */
[[nodiscard]] std::vector<double> spin_go_payouts(double multiplier, double buyin, bool winner_take_all);

/// Harville ICM $EV for three stacks and a length-3 prize vector (`icm_expected_payouts`).
[[nodiscard]] std::vector<double> spin_go_icm_ev(const std::vector<double>& stacks,
                                                 const std::vector<double>& payouts);

struct SpinGoNashJamCallResult {
    std::array<double, kNashHandCount> jam{};
    std::array<double, kNashHandCount> sb_call{};
    std::array<double, kNashHandCount> bb_call{};
    int iterations{0};
};

/**
 * 3-handed first-in Nash jam/call with ICM utility (not chip EV).
 *
 * Approximation: BTN open-jams; SB then BB call sequentially (`nash_multiway_shove_call`).
 * Blinds are dead in the pot; they are not deducted again from SB/BB stacks.
 */
[[nodiscard]] SpinGoNashJamCallResult spin_go_nash_jam_call(double btn_stack, double sb_stack,
                                                            double bb_stack,
                                                            const std::vector<double>& payouts,
                                                            double small_blind, double big_blind,
                                                            double ante);

/**
 * Average-position FGS orbits on stacks, then ICMBU (`pko_icmbu_payouts`) on survivors.
 * `orbits == 0` matches `pko_icmbu_payouts` on the original stacks.
 */
[[nodiscard]] PkoIcmbuResult pko_fgs_payouts(const std::vector<double>& stacks,
                                             const std::vector<double>& payouts,
                                             const std::vector<double>& bounty_values, int orbits,
                                             double small_blind, double big_blind, double ante);

struct LateRegOverlayResult {
    double overlay_ratio{0.0};
    double register_ev{0.0};
    double icm_share{0.0};
};

/**
 * Overlay = `(prizePool / fieldRemaining) / lateRegFee`.
 * $EV of sitting now: Harville on a compressed table (you / one average / rest of field),
 * then subtract the fee. Pool after you pay is `prizePool + lateRegFee`.
 */
[[nodiscard]] LateRegOverlayResult late_reg_overlay_ev(int field_remaining, double prize_pool,
                                                       double late_reg_fee, double starting_stack,
                                                       double average_stack);

struct SatelliteTicketEv {
    double advance_prob{0.0};
    double ticket_ev{0.0};
    double chip_ev_if_double{0.0};
    double dollar_ev_if_double{0.0};
};

/**
 * WTA satellite: P(top-K ticket) via `icm_satellite_advance_probability`.
 * `ticketEv = P * ticketValue`. Doubling hero's stack: chip-share of the ticket pool vs
 * Harville $EV after the double.
 */
[[nodiscard]] SatelliteTicketEv winner_take_all_satellite_ev(const std::vector<double>& stacks,
                                                             std::size_t hero, int ticket_count,
                                                             double ticket_value);

struct SpotChipEv {
    double take_ev{0.0};
    double fold_ev{0.0};
    double delta{0.0};
};

/**
 * Squeeze vs fold (chip EV). Fold = 0 (hero has not put chips in).
 * Independent folds: both / opener-only / caller-only / both continue.
 * Continue pots: `pot + heroPut + matching calls`. Showdown EV = equity * finalPot − heroPut.
 */
[[nodiscard]] SpotChipEv squeeze_ev(double pot, double hero_put, double opener_call, double caller_call,
                                    double fold_equity_opener, double fold_equity_caller,
                                    double equity_vs_opener, double equity_vs_caller,
                                    double equity_vs_both);

/**
 * 4-bet jam pot geometry (not ICM). Fold = 0. FE=1 wins `dead_pot`.
 * Called: `equity * (deadPot + jam + call) − jam`.
 */
[[nodiscard]] SpotChipEv four_bet_jam_ev(double dead_pot, double jam, double call, double fold_equity,
                                         double equity_when_called);

struct IsoRaiseEv {
    double iso_ev{0.0};
    double check_ev{0.0};
    double fold_ev{0.0};
};

/**
 * Isolate vs `n` limpers. Each folds independently with `p_fold`.
 * Vs `k` callers: `equities[k-1]` if provided, else `1/(k+1)`.
 * Check-behind: `1/(n+1)` of the current pot, no extra chips.
 */
[[nodiscard]] IsoRaiseEv iso_raise_vs_limpers_ev(double pot, double iso_size, double limp_call,
                                                 int n_limpers, double p_fold,
                                                 const std::vector<double>& equities);

struct ThreeBetCommitEv {
    double spr{0.0};
    bool stack_off{false};
    double continue_ev{0.0};
    double fold_ev{0.0};
};

/**
 * SPR after the 3-bet (`spr(pot, remaining)`). Realized equity = `equity * realization`.
 * Stack-off when realized equity covers pot odds `remaining / (pot + 2*remaining)`.
 */
[[nodiscard]] ThreeBetCommitEv three_bet_pot_commit_ev(double pot_after_three_bet,
                                                       double effective_remaining, double equity,
                                                       double realization);

}  // namespace poker
