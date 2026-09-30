#pragma once

#include "poker/pko.hpp"

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

/**
 * Average-position FGS orbits on stacks, then ICMBU (`pko_icmbu_payouts`) on survivors.
 * `orbits == 0` matches `pko_icmbu_payouts` on the original stacks.
 */
[[nodiscard]] PkoIcmbuResult pko_fgs_payouts(const std::vector<double>& stacks,
                                             const std::vector<double>& payouts,
                                             const std::vector<double>& bounty_values, int orbits,
                                             double small_blind, double big_blind, double ante);

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
