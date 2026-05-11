#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
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

}  // namespace poker
