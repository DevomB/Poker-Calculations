#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace poker {

namespace {

[[nodiscard]] bool is_finite_non_neg(double x) {
    return std::isfinite(x) && x >= 0.0;
}

[[nodiscard]] bool is_finite_pos(double x) {
    return std::isfinite(x) && x > 0.0;
}

void assert_non_neg_finite(const char* name, double x) {
    if (!is_finite_non_neg(x)) {
        throw std::invalid_argument(std::string(name) + " must be a finite non-negative number");
    }
}

void assert_positive_finite(const char* name, double x) {
    if (!is_finite_pos(x)) {
        throw std::invalid_argument(std::string(name) + " must be a finite positive number");
    }
}

[[nodiscard]] double clamp01(double x) {
    return std::clamp(x, 0.0, 1.0);
}

}  // namespace

double pot_odds_ratio(int pot, int to_call) {
    if (to_call <= 0) {
        return 0.0;
    }
    const double denom = static_cast<double>(pot) + static_cast<double>(to_call);
    if (denom <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(to_call) / denom;
}

double expected_value_call(double equity, int pot, int to_call) {
    if (to_call <= 0) {
        return equity * static_cast<double>(pot);
    }
    const double win = static_cast<double>(pot + to_call);
    const double lose = static_cast<double>(to_call);
    return equity * win - (1.0 - equity) * lose;
}

double spr(double pot_chips, double effective_stack_chips) {
    assert_non_neg_finite("potChips", pot_chips);
    assert_non_neg_finite("effectiveStackChips", effective_stack_chips);
    if (pot_chips == 0.0) {
        return effective_stack_chips == 0.0 ? 0.0 : std::numeric_limits<double>::infinity();
    }
    return effective_stack_chips / pot_chips;
}

double effective_stack(const std::vector<double>& stacks) {
    if (stacks.empty()) {
        return 0.0;
    }
    double m = stacks[0];
    assert_non_neg_finite("stack", m);
    for (std::size_t i = 1; i < stacks.size(); ++i) {
        const double x = stacks[i];
        assert_non_neg_finite("stack", x);
        m = std::min(m, x);
    }
    return m;
}

double breakeven_call_equity(double pot_before_call, double to_call) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    const double denom = pot_before_call + to_call;
    if (denom <= 0.0 || to_call == 0.0) {
        return 0.0;
    }
    return to_call / denom;
}

double minimum_defense_frequency(double pot_before_opponent_bet, double opponent_bet_size) {
    assert_non_neg_finite("potBeforeOpponentBet", pot_before_opponent_bet);
    assert_non_neg_finite("opponentBetSize", opponent_bet_size);
    const double denom = pot_before_opponent_bet + opponent_bet_size;
    return denom <= 0.0 ? 0.0 : pot_before_opponent_bet / denom;
}

double stack_in_big_blinds(double stack_chips, double big_blind) {
    assert_non_neg_finite("stackChips", stack_chips);
    assert_positive_finite("bigBlind", big_blind);
    return stack_chips / big_blind;
}

double pot_odds_ratio_display(double pot_before_call, double to_call) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    if (to_call == 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return pot_before_call / to_call;
}

std::string format_pot_odds(double pot_before_call, double to_call, int decimals) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    if (to_call == 0.0) {
        return "\u221e:1";
    }
    if (decimals < 0 || decimals > 18) {
        throw std::invalid_argument("decimals must be between 0 and 18");
    }
    const double r = pot_before_call / to_call;
    const double scale = std::pow(10.0, static_cast<double>(decimals));
    const double f = std::round(r * scale) / scale;
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(static_cast<int>(decimals));
    oss << f << ":1";
    return oss.str();
}

double rule_of_four_equity(double outs) {
    assert_non_neg_finite("outs", outs);
    const double o = std::min(outs, 48.0);
    return std::min(1.0, (o * 4.0) / 100.0);
}

double rule_of_two_equity(double outs) {
    assert_non_neg_finite("outs", outs);
    const double o = std::min(outs, 48.0);
    return std::min(1.0, (o * 2.0) / 100.0);
}

double implied_breakeven_future_win(double pot_before_call, double to_call, double equity) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be a finite number");
    }
    const double e = clamp01(equity);
    if (e <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    const double immediate_win_total = pot_before_call + to_call;
    return std::max(0.0, ((1.0 - e) * to_call) / e - immediate_win_total);
}

double bluff_to_value_ratio(double pot_before_bet, double bet_size) {
    assert_non_neg_finite("potBeforeBet", pot_before_bet);
    assert_non_neg_finite("betSize", bet_size);
    const double denom = pot_before_bet + 2.0 * bet_size;
    return denom <= 0.0 ? 0.0 : bet_size / denom;
}

double value_to_bluff_ratio(double pot_before_bet, double bet_size) {
    const double b = bluff_to_value_ratio(pot_before_bet, bet_size);
    if (b == 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return 1.0 / b;
}

double bet_as_pot_fraction(double pot_before_bet, double bet_size) {
    assert_non_neg_finite("potBeforeBet", pot_before_bet);
    assert_non_neg_finite("betSize", bet_size);
    if (pot_before_bet == 0.0) {
        return bet_size > 0.0 ? std::numeric_limits<double>::infinity() : 0.0;
    }
    return bet_size / pot_before_bet;
}

double spr_after_call(double pot_before_call, double to_call, double effective_stack_before_call) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    assert_non_neg_finite("effectiveStackBeforeCall", effective_stack_before_call);
    if (to_call > effective_stack_before_call) {
        throw std::invalid_argument("toCall cannot exceed effectiveStackBeforeCall for sprAfterCall");
    }
    const double stack_after = effective_stack_before_call - to_call;
    const double new_pot = pot_before_call + 2.0 * to_call;
    if (new_pot <= 0.0) {
        return stack_after > 0.0 ? std::numeric_limits<double>::infinity() : 0.0;
    }
    return stack_after / new_pot;
}

double commitment_ratio(double to_call, double effective_stack_before_call) {
    assert_non_neg_finite("toCall", to_call);
    assert_non_neg_finite("effectiveStackBeforeCall", effective_stack_before_call);
    if (effective_stack_before_call == 0.0) {
        return 0.0;
    }
    return to_call / effective_stack_before_call;
}

double alpha_frequency(double pot_before_bet, double bet_size) {
    assert_non_neg_finite("potBeforeBet", pot_before_bet);
    assert_non_neg_finite("betSize", bet_size);
    const double denom = pot_before_bet + bet_size;
    return denom <= 0.0 ? 0.0 : bet_size / denom;
}

double breakeven_fold_equity_pure_bluff(double pot_before_hero_bet, double hero_bet_or_call_size) {
    assert_non_neg_finite("potBeforeHeroBet", pot_before_hero_bet);
    assert_non_neg_finite("heroBetOrCallSize", hero_bet_or_call_size);
    const double denom = pot_before_hero_bet + hero_bet_or_call_size;
    return denom <= 0.0 ? 0.0 : hero_bet_or_call_size / denom;
}

double breakeven_fold_equity_semi_bluff(double pot_before_hero_bet, double hero_bet_size,
                                       double equity_when_called, double total_pot_if_called) {
    assert_non_neg_finite("potBeforeHeroBet", pot_before_hero_bet);
    assert_non_neg_finite("heroBetSize", hero_bet_size);
    if (!std::isfinite(equity_when_called)) {
        throw std::invalid_argument("equityWhenCalled must be a finite number");
    }
    assert_positive_finite("totalPotIfCalled", total_pot_if_called);
    const double e = clamp01(equity_when_called);
    const double net_when_called = e * total_pot_if_called - hero_bet_size;
    if (net_when_called >= 0.0) {
        return 0.0;
    }
    const double den = net_when_called - pot_before_hero_bet;
    if (std::abs(den) < 1e-15) {
        throw std::invalid_argument("breakevenFoldEquitySemiBluff: degenerate (net equals pot)");
    }
    return net_when_called / den;
}

double hypergeometric_one_card_hit_probability(double outs, double unseen_cards) {
    assert_non_neg_finite("outs", outs);
    assert_positive_finite("unseenCards", unseen_cards);
    if (outs > unseen_cards) {
        throw std::invalid_argument("outs cannot exceed unseenCards for one-card draw");
    }
    return outs / unseen_cards;
}

double runner_runner_flush_two_card_probability(double suit_cards_remaining, double unseen_cards) {
    assert_non_neg_finite("suitCardsRemaining", suit_cards_remaining);
    assert_positive_finite("unseenCards", unseen_cards);
    if (suit_cards_remaining > unseen_cards) {
        throw std::invalid_argument("suitCardsRemaining cannot exceed unseenCards");
    }
    if (suit_cards_remaining < 2.0) {
        return 0.0;
    }
    if (unseen_cards < 2.0) {
        return 0.0;
    }
    const double num = suit_cards_remaining * (suit_cards_remaining - 1.0);
    const double den = unseen_cards * (unseen_cards - 1.0);
    return den <= 0.0 ? 0.0 : num / den;
}

double flop_to_river_at_least_one_hit_probability(double outs, double unseen_after_flop) {
    assert_non_neg_finite("outs", outs);
    assert_positive_finite("unseenAfterFlop", unseen_after_flop);
    if (outs > unseen_after_flop) {
        throw std::invalid_argument("outs cannot exceed unseenAfterFlop");
    }
    if (unseen_after_flop < 2.0) {
        throw std::invalid_argument("need at least two unseen cards for flop-to-river");
    }
    if (outs <= 0.0) {
        return 0.0;
    }
    const double u = unseen_after_flop;
    const double o = outs;
    const double miss_both = ((u - o) / u) * ((u - o - 1.0) / (u - 1.0));
    return 1.0 - miss_both;
}

double flop_to_river_two_category_union_hit_probability(double unseen_after_flop, double outs_a,
                                                       double outs_b, double overlap_ab) {
    assert_positive_finite("unseenAfterFlop", unseen_after_flop);
    assert_non_neg_finite("outsA", outs_a);
    assert_non_neg_finite("outsB", outs_b);
    assert_non_neg_finite("overlapAb", overlap_ab);
    if (overlap_ab > outs_a || overlap_ab > outs_b) {
        throw std::invalid_argument("overlapAb cannot exceed outsA or outsB");
    }
    const double union_outs = outs_a + outs_b - overlap_ab;
    return flop_to_river_at_least_one_hit_probability(union_outs, unseen_after_flop);
}

double flop_to_river_at_least_one_hit_union_two_categories(double unseen_after_flop, double outs_a,
                                                          double outs_b, double shared_ab) {
    return flop_to_river_two_category_union_hit_probability(unseen_after_flop, outs_a, outs_b,
                                                            shared_ab);
}

double flop_to_river_at_least_one_hit_union_three_categories(double unseen_after_flop, double outs_a,
                                                             double outs_b, double outs_c, double shared_ab,
                                                             double shared_ac, double shared_bc,
                                                             double shared_abc) {
    assert_positive_finite("unseenAfterFlop", unseen_after_flop);
    assert_non_neg_finite("outsA", outs_a);
    assert_non_neg_finite("outsB", outs_b);
    assert_non_neg_finite("outsC", outs_c);
    assert_non_neg_finite("sharedAb", shared_ab);
    assert_non_neg_finite("sharedAc", shared_ac);
    assert_non_neg_finite("sharedBc", shared_bc);
    assert_non_neg_finite("sharedAbc", shared_abc);
    const double union_outs =
        outs_a + outs_b + outs_c - shared_ab - shared_ac - shared_bc + shared_abc;
    if (!std::isfinite(union_outs) || union_outs < 0.0) {
        throw std::invalid_argument("computed union outs must be non-negative and finite");
    }
    return flop_to_river_at_least_one_hit_probability(union_outs, unseen_after_flop);
}

double flop_to_river_at_least_one_hit_union_four_categories(
    double unseen_after_flop, double oa, double ob, double oc, double od, double s01, double s02,
    double s03, double s12, double s13, double s23, double s012, double s013, double s023, double s123,
    double four_way) {
    assert_positive_finite("unseenAfterFlop", unseen_after_flop);
    assert_non_neg_finite("outsA", oa);
    assert_non_neg_finite("outsB", ob);
    assert_non_neg_finite("outsC", oc);
    assert_non_neg_finite("outsD", od);
    assert_non_neg_finite("s01", s01);
    assert_non_neg_finite("s02", s02);
    assert_non_neg_finite("s03", s03);
    assert_non_neg_finite("s12", s12);
    assert_non_neg_finite("s13", s13);
    assert_non_neg_finite("s23", s23);
    assert_non_neg_finite("s012", s012);
    assert_non_neg_finite("s013", s013);
    assert_non_neg_finite("s023", s023);
    assert_non_neg_finite("s123", s123);
    assert_non_neg_finite("fourWay", four_way);
    const double pair_sum = s01 + s02 + s03 + s12 + s13 + s23;
    const double triple_sum = s012 + s013 + s023 + s123;
    const double union_outs = oa + ob + oc + od - pair_sum + triple_sum - four_way;
    if (!std::isfinite(union_outs) || union_outs < 0.0) {
        throw std::invalid_argument("computed union outs must be non-negative and finite");
    }
    return flop_to_river_at_least_one_hit_probability(union_outs, unseen_after_flop);
}

double runner_runner_straight_draw_hit_probability(Runner_runner_straight_draw_kind kind,
                                                  int dead_cards_among_pattern_outs,
                                                  double unseen_after_flop) {
    if (dead_cards_among_pattern_outs < 0) {
        throw std::invalid_argument("deadCardsAmongPatternOuts must be non-negative");
    }
    int base = 4;
    switch (kind) {
        case Runner_runner_straight_draw_kind::GutshotFourOut:
            base = 4;
            break;
        case Runner_runner_straight_draw_kind::OpenEndedEightOut:
        case Runner_runner_straight_draw_kind::DoubleBellyBusterEightOut:
            base = 8;
            break;
    }
    const double outs = static_cast<double>(base - dead_cards_among_pattern_outs);
    return flop_to_river_at_least_one_hit_probability(outs, unseen_after_flop);
}

double reverse_implied_odds_max_future_loss(double pot_before_call, double to_call, double equity) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be a finite number");
    }
    const double e = clamp01(equity);
    if (e <= 0.0) {
        return 0.0;
    }
    if (e >= 1.0) {
        return std::numeric_limits<double>::infinity();
    }
    const double immediate_win = pot_before_call + to_call;
    const double ev_at_zero_loss = e * immediate_win - (1.0 - e) * to_call;
    if (ev_at_zero_loss < 0.0) {
        return 0.0;
    }
    const double max_loss = (e * immediate_win) / (1.0 - e) - to_call;
    return std::max(0.0, max_loss);
}

double geometric_pot_after_matched_pot_fractions(double pot0, double fraction, int n_rounds) {
    assert_non_neg_finite("pot0", pot0);
    if (!std::isfinite(fraction) || fraction < 0.0) {
        throw std::invalid_argument("fraction must be a finite non-negative number");
    }
    if (n_rounds < 0) {
        throw std::invalid_argument("nRounds must be non-negative");
    }
    if (pot0 == 0.0) {
        return 0.0;
    }
    const double factor = 1.0 + 2.0 * fraction;
    return pot0 * std::pow(factor, static_cast<double>(n_rounds));
}

double harrington_m(double stack_chips, double small_blind, double big_blind, double total_antes) {
    assert_non_neg_finite("stackChips", stack_chips);
    assert_non_neg_finite("smallBlind", small_blind);
    assert_non_neg_finite("bigBlind", big_blind);
    assert_non_neg_finite("totalAntes", total_antes);
    const double denom = small_blind + big_blind + total_antes;
    if (denom <= 0.0) {
        throw std::invalid_argument("Harrington M denominator (sb+bb+antes) must be positive");
    }
    return stack_chips / denom;
}

double harrington_m_effective(double stack_chips, double small_blind, double big_blind,
                              double ante_per_active_player, int num_active_players) {
    assert_non_neg_finite("stackChips", stack_chips);
    assert_non_neg_finite("smallBlind", small_blind);
    assert_non_neg_finite("bigBlind", big_blind);
    assert_non_neg_finite("antePerActivePlayer", ante_per_active_player);
    if (num_active_players < 1) {
        throw std::invalid_argument("numActivePlayers must be at least 1");
    }
    const double total_antes = ante_per_active_player * static_cast<double>(num_active_players);
    return harrington_m(stack_chips, small_blind, big_blind, total_antes);
}

double harrington_m_effective_active_antes(double stack_chips, double small_blind, double big_blind,
                                          const std::vector<double>& antes_from_active_seats) {
    double total_antes = 0.0;
    for (double a : antes_from_active_seats) {
        assert_non_neg_finite("anteSeat", a);
        total_antes += a;
    }
    return harrington_m(stack_chips, small_blind, big_blind, total_antes);
}

double kelly_criterion_binary(double win_probability, double net_odds) {
    if (!std::isfinite(win_probability)) {
        throw std::invalid_argument("winProbability must be finite");
    }
    assert_positive_finite("netOdds", net_odds);
    const double p = clamp01(win_probability);
    const double q = 1.0 - p;
    return (p * net_odds - q) / net_odds;
}

double monte_carlo_standard_error(double p_hat, int n_trials) {
    if (!std::isfinite(p_hat)) {
        throw std::invalid_argument("pHat must be finite");
    }
    if (n_trials <= 0) {
        throw std::invalid_argument("nTrials must be positive");
    }
    const double p = clamp01(p_hat);
    return std::sqrt(p * (1.0 - p) / static_cast<double>(n_trials));
}

Beta_binomial_fold_posterior beta_binomial_fold_update(double prior_alpha, double prior_beta,
                                                       int folds, int calls) {
    if (!std::isfinite(prior_alpha) || prior_alpha <= 0.0) {
        throw std::invalid_argument("priorAlpha must be finite and positive");
    }
    if (!std::isfinite(prior_beta) || prior_beta <= 0.0) {
        throw std::invalid_argument("priorBeta must be finite and positive");
    }
    if (folds < 0 || calls < 0) {
        throw std::invalid_argument("folds and calls must be non-negative");
    }
    const double a = prior_alpha + static_cast<double>(folds);
    const double b = prior_beta + static_cast<double>(calls);
    Beta_binomial_fold_posterior out{};
    out.alpha = a;
    out.beta = b;
    out.posterior_mean = a / (a + b);
    return out;
}

double duplication_adjusted_outs(double outs, int num_villains, double duplication_weight) {
    assert_non_neg_finite("outs", outs);
    if (num_villains < 0) {
        throw std::invalid_argument("numVillains must be non-negative");
    }
    assert_non_neg_finite("duplicationWeight", duplication_weight);
    const double denom = 1.0 + duplication_weight * static_cast<double>(num_villains);
    return denom <= 0.0 ? outs : outs / denom;
}

double risk_of_ruin_diffusion_approx(double drift_per_hand, double variance_per_hand,
                                     double bankroll) {
    if (!std::isfinite(drift_per_hand) || !std::isfinite(variance_per_hand) ||
        !std::isfinite(bankroll)) {
        throw std::invalid_argument("drift, variance, and bankroll must be finite");
    }
    assert_positive_finite("bankroll", bankroll);
    assert_positive_finite("variancePerHand", variance_per_hand);
    if (drift_per_hand <= 0.0) {
        return 1.0;
    }
    const double exponent = -2.0 * drift_per_hand * bankroll / variance_per_hand;
    return clamp01(std::exp(exponent));
}

double bankroll_for_target_ror_diffusion(double drift_per_hand, double variance_per_hand,
                                         double target_ror) {
    if (!std::isfinite(drift_per_hand) || !std::isfinite(variance_per_hand) ||
        !std::isfinite(target_ror)) {
        throw std::invalid_argument("arguments must be finite");
    }
    assert_positive_finite("variancePerHand", variance_per_hand);
    assert_positive_finite("driftPerHand", drift_per_hand);
    if (target_ror <= 0.0 || target_ror > 1.0) {
        throw std::invalid_argument("targetRor must be in (0, 1]");
    }
    if (target_ror >= 1.0) {
        return 0.0;
    }
    const double b =
        -std::log(target_ror) * variance_per_hand / (2.0 * drift_per_hand);
    return std::max(0.0, b);
}

Wilson_interval wilson_score_interval(int successes, int n_trials, double z) {
    if (successes < 0 || n_trials < 0) {
        throw std::invalid_argument("successes and nTrials must be non-negative");
    }
    if (successes > n_trials) {
        throw std::invalid_argument("successes cannot exceed nTrials");
    }
    if (!std::isfinite(z) || z <= 0.0) {
        throw std::invalid_argument("z must be finite and positive");
    }
    Wilson_interval w{};
    if (n_trials == 0) {
        w.lower = 0.0;
        w.upper = 1.0;
        return w;
    }
    const double n = static_cast<double>(n_trials);
    const double p = static_cast<double>(successes) / n;
    const double z2 = z * z;
    const double denom = 1.0 + z2 / n;
    const double center = (p + z2 / (2.0 * n)) / denom;
    const double half = z * std::sqrt((p * (1.0 - p) + z2 / (4.0 * n * n)) / n) / denom;
    w.lower = clamp01(center - half);
    w.upper = clamp01(center + half);
    return w;
}

double rake_from_pot(double pot_chips, double rake_fraction, double rake_cap) {
    assert_non_neg_finite("potChips", pot_chips);
    assert_non_neg_finite("rakeFraction", rake_fraction);
    assert_non_neg_finite("rakeCap", rake_cap);
    if (rake_fraction > 1.0) {
        throw std::invalid_argument("rakeFraction should not exceed 1 for this model");
    }
    const double raw = rake_fraction * pot_chips;
    return std::min(raw, rake_cap);
}

double breakeven_call_equity_with_rake(double pot_before_call, double to_call, double rake_fraction,
                                       double rake_cap) {
    assert_non_neg_finite("potBeforeCall", pot_before_call);
    assert_non_neg_finite("toCall", to_call);
    const double final_pot = pot_before_call + 2.0 * to_call;
    const double rake = rake_from_pot(final_pot, rake_fraction, rake_cap);
    const double denom = pot_before_call + 2.0 * to_call - rake;
    if (denom <= 0.0) {
        throw std::invalid_argument("rake model leaves no positive pot for breakeven equity");
    }
    return to_call / denom;
}

double breakeven_fold_equity_semi_bluff_with_rake(double pot_before_hero_bet, double hero_bet_size,
                                                 double equity_when_called, double total_pot_if_called,
                                                 double rake_fraction, double rake_cap) {
    assert_non_neg_finite("potBeforeHeroBet", pot_before_hero_bet);
    assert_non_neg_finite("heroBetSize", hero_bet_size);
    if (!std::isfinite(equity_when_called)) {
        throw std::invalid_argument("equityWhenCalled must be finite");
    }
    assert_positive_finite("totalPotIfCalled", total_pot_if_called);
    const double e = clamp01(equity_when_called);
    const double rake = rake_from_pot(total_pot_if_called, rake_fraction, rake_cap);
    const double net_when_called =
        e * (total_pot_if_called - rake) - hero_bet_size;
    if (net_when_called >= 0.0) {
        return 0.0;
    }
    const double den = net_when_called - pot_before_hero_bet;
    if (std::abs(den) < 1e-15) {
        throw std::invalid_argument("breakevenFoldEquitySemiBluffWithRake: degenerate (net equals pot)");
    }
    return net_when_called / den;
}

double breakeven_fold_equity_pure_bluff_with_rake(double pot_before_hero_bet, double hero_bet_or_call_size,
                                                 double rake_fraction, double rake_cap) {
    assert_non_neg_finite("potBeforeHeroBet", pot_before_hero_bet);
    assert_non_neg_finite("heroBetOrCallSize", hero_bet_or_call_size);
    const double shipped = pot_before_hero_bet + hero_bet_or_call_size;
    const double rake = rake_from_pot(shipped, rake_fraction, rake_cap);
    const double win_net = shipped - rake;
    if (win_net + hero_bet_or_call_size <= 0.0) {
        throw std::invalid_argument("pure bluff with rake: win net plus risk must be positive");
    }
    return hero_bet_or_call_size / (win_net + hero_bet_or_call_size);
}

double multiway_symmetric_breakeven_call_equity(double pot_before, double to_call,
                                              int symmetric_extra_callers) {
    return multiway_symmetric_breakeven_call_equity_with_share(
        pot_before, to_call, symmetric_extra_callers, Multiway_symmetric_pot_share_model::WinnerTakesAll,
        1.0);
}

double multiway_symmetric_breakeven_call_equity_with_share(double pot_before, double to_call,
                                                          int symmetric_extra_callers,
                                                          Multiway_symmetric_pot_share_model model,
                                                          double hero_fraction_of_pot_when_win) {
    assert_non_neg_finite("potBefore", pot_before);
    assert_non_neg_finite("toCall", to_call);
    if (symmetric_extra_callers < 0) {
        throw std::invalid_argument("symmetricExtraCallers must be non-negative");
    }
    const double k = static_cast<double>(symmetric_extra_callers);
    const double denom = pot_before + to_call * (1.0 + k);
    if (denom <= 0.0 || to_call == 0.0) {
        return 0.0;
    }
    double share = 1.0;
    if (model == Multiway_symmetric_pot_share_model::WinnerTakesAll) {
        share = 1.0;
    } else if (model == Multiway_symmetric_pot_share_model::FixedHeroShareWhenWins) {
        if (!std::isfinite(hero_fraction_of_pot_when_win) || hero_fraction_of_pot_when_win <= 0.0 ||
            hero_fraction_of_pot_when_win > 1.0) {
            throw std::invalid_argument("heroFractionOfPotWhenWin must be in (0, 1] for FixedHeroShareWhenWins");
        }
        share = hero_fraction_of_pot_when_win;
    } else {
        throw std::invalid_argument("unknown Multiway_symmetric_pot_share_model");
    }
    const double eff = share * denom;
    if (eff <= 0.0) {
        throw std::invalid_argument("effective weighted pot must be positive");
    }
    return to_call / eff;
}

double two_street_pure_bluff_ev(double pot_before_street1, double bet_street1, double bet_street2,
                               double fold_equity_street1, double fold_equity_street2) {
    assert_non_neg_finite("potBeforeStreet1", pot_before_street1);
    assert_non_neg_finite("betStreet1", bet_street1);
    assert_non_neg_finite("betStreet2", bet_street2);
    if (!std::isfinite(fold_equity_street1) || !std::isfinite(fold_equity_street2)) {
        throw std::invalid_argument("fold equities must be finite");
    }
    const double fe1 = clamp01(fold_equity_street1);
    const double fe2 = clamp01(fold_equity_street2);
    const double P0 = pot_before_street1;
    const double B1 = bet_street1;
    const double B2 = bet_street2;
    const double A = P0 + B1 - B2;
    const double C = B1 + B2;
    return fe1 * P0 + (1.0 - fe1) * (-B1 + fe2 * A - (1.0 - fe2) * C);
}

double breakeven_fold_equity_second_street_pure_bluff(double pot_before_street1, double bet_street1,
                                                     double bet_street2, double fold_equity_street1) {
    assert_non_neg_finite("potBeforeStreet1", pot_before_street1);
    assert_non_neg_finite("betStreet1", bet_street1);
    assert_non_neg_finite("betStreet2", bet_street2);
    if (!std::isfinite(fold_equity_street1)) {
        throw std::invalid_argument("foldEquityStreet1 must be finite");
    }
    const double fe1 = clamp01(fold_equity_street1);
    if (std::abs(1.0 - fe1) < 1e-15) {
        throw std::invalid_argument("foldEquityStreet1 cannot be 1 (degenerate)");
    }
    const double P0 = pot_before_street1;
    const double B1 = bet_street1;
    const double B2 = bet_street2;
    const double A = P0 + B1 - B2;
    const double C = B1 + B2;
    const double sum = A + C;
    if (std::abs(sum) < 1e-15) {
        throw std::invalid_argument("degenerate pot/bet geometry for second-street breakeven");
    }
    const double rhs = C - B1 - fe1 * P0 / (1.0 - fe1);
    return rhs / sum;
}

double two_street_pure_bluff_same_fold_equity(double pot_before_street1, double bet_street1,
                                             double bet_street2) {
    assert_non_neg_finite("potBeforeStreet1", pot_before_street1);
    assert_non_neg_finite("betStreet1", bet_street1);
    assert_non_neg_finite("betStreet2", bet_street2);
    const double P0 = pot_before_street1;
    const double B1 = bet_street1;
    const double B2 = bet_street2;
    const double Bsum = B1 + B2;
    const double A = P0 + 2.0 * B1;
    const double a = -A;
    const double b = 2.0 * P0 + 3.0 * B1 + B2;
    const double c = -Bsum;
    if (std::abs(a) < 1e-18) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double s = std::sqrt(disc);
    const double x1 = (-b + s) / (2.0 * a);
    const double x2 = (-b - s) / (2.0 * a);
    auto pick = [](double x1, double x2) -> double {
        const bool g1 = x1 >= 0.0 && x1 <= 1.0;
        const bool g2 = x2 >= 0.0 && x2 <= 1.0;
        if (g1 && g2) {
            return std::min(x1, x2);
        }
        if (g1) {
            return x1;
        }
        if (g2) {
            return x2;
        }
        return std::numeric_limits<double>::quiet_NaN();
    };
    return pick(x1, x2);
}

double breakeven_fold_equity_first_street_pure_bluff(double pot_before_street1, double bet_street1,
                                                    double bet_street2, double fold_equity_street2) {
    assert_non_neg_finite("potBeforeStreet1", pot_before_street1);
    assert_non_neg_finite("betStreet1", bet_street1);
    assert_non_neg_finite("betStreet2", bet_street2);
    if (!std::isfinite(fold_equity_street2)) {
        throw std::invalid_argument("foldEquityStreet2 must be finite");
    }
    const double fe2 = clamp01(fold_equity_street2);
    const double P0 = pot_before_street1;
    const double B1 = bet_street1;
    const double B2 = bet_street2;
    const double A = P0 + B1 - B2;
    const double C = B1 + B2;
    const double M = -B1 + fe2 * A - (1.0 - fe2) * C;
    const double den = M - P0;
    if (std::abs(den) < 1e-15) {
        throw std::invalid_argument("degenerate pot/bet geometry for first-street breakeven");
    }
    return M / den;
}

double flop_to_river_at_least_one_hit_disjoint_outs_sum(
    double unseen_after_flop, const std::vector<double>& outs_per_disjoint_category) {
    assert_positive_finite("unseenAfterFlop", unseen_after_flop);
    double sum = 0.0;
    for (double o : outs_per_disjoint_category) {
        assert_non_neg_finite("outsCategory", o);
        sum += o;
    }
    if (sum > unseen_after_flop) {
        throw std::invalid_argument("sum of disjoint outs cannot exceed unseenAfterFlop");
    }
    return flop_to_river_at_least_one_hit_probability(sum, unseen_after_flop);
}

double chubukov_symmetric_jam_ev(double jam_stack_chips, double dead_money_chips, double equity) {
    assert_non_neg_finite("jamStackChips", jam_stack_chips);
    assert_non_neg_finite("deadMoneyChips", dead_money_chips);
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be finite");
    }
    const double e = clamp01(equity);
    return e * (2.0 * jam_stack_chips + dead_money_chips) - jam_stack_chips;
}

int chubukov_max_symmetric_jam_stack_chips_binary_search(double equity, double dead_money_chips,
                                                         int max_stack_chips) {
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be finite");
    }
    assert_non_neg_finite("deadMoneyChips", dead_money_chips);
    if (max_stack_chips < 1) {
        return 0;
    }
    const double e = clamp01(equity);
    if (e > 0.5) {
        return max_stack_chips;
    }
    if (e <= 0.0) {
        return 0;
    }
    auto ev_at = [&](int s) { return chubukov_symmetric_jam_ev(static_cast<double>(s), dead_money_chips, e); };
    if (ev_at(1) < 0.0) {
        return 0;
    }
    int lo = 1;
    int hi = max_stack_chips;
    int ans = 1;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        if (ev_at(mid) >= 0.0) {
            ans = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return ans;
}

double chubukov_symmetric_jam_breakeven_stack(double dead_money_chips, double equity) {
    assert_non_neg_finite("deadMoneyChips", dead_money_chips);
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be finite");
    }
    const double e = clamp01(equity);
    if (e <= 0.0) {
        return 0.0;
    }
    if (e > 0.5) {
        return std::numeric_limits<double>::infinity();
    }
    if (std::abs(e - 0.5) < 1e-15) {
        return dead_money_chips > 0.0 ? std::numeric_limits<double>::infinity() : 0.0;
    }
    return e * dead_money_chips / (1.0 - 2.0 * e);
}

double chubukov_symmetric_jam_ev(double jam_stack_chips, double dead_money_chips, double equity) {
    assert_non_neg_finite("jamStackChips", jam_stack_chips);
    assert_non_neg_finite("deadMoneyChips", dead_money_chips);
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be finite");
    }
    const double e = clamp01(equity);
    return e * (2.0 * jam_stack_chips + dead_money_chips) - jam_stack_chips;
}

int chubukov_max_symmetric_jam_stack_chips_binary_search(double equity, double dead_money_chips,
                                                        int max_stack_chips) {
    if (!std::isfinite(equity)) {
        throw std::invalid_argument("equity must be finite");
    }
    assert_non_neg_finite("deadMoneyChips", dead_money_chips);
    if (max_stack_chips < 0) {
        throw std::invalid_argument("maxStackChips must be non-negative");
    }
    if (max_stack_chips == 0) {
        return 0;
    }
    const double e = clamp01(equity);
    if (e <= 0.0) {
        return 0;
    }
    if (e >= 0.5) {
        return max_stack_chips;
    }
    const auto ev = [e, dead_money_chips](int s) {
        return chubukov_symmetric_jam_ev(static_cast<double>(s), dead_money_chips, e);
    };
    if (ev(1) < 0.0) {
        return 0;
    }
    if (ev(max_stack_chips) >= 0.0) {
        return max_stack_chips;
    }
    int lo = 1;
    int hi = max_stack_chips;
    while (lo < hi) {
        const int mid = lo + (hi - lo + 1) / 2;
        if (ev(mid) >= 0.0) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

}  // namespace poker
