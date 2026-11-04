#include "poker/strategy.hpp"

#include "poker/cancel.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/poker_math.hpp"
#include "poker/side_pot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace poker {

namespace {

/// Pot-before-call and the chips a call actually costs. A full call keeps the package convention
/// (`pot` includes villain's bet). A short stack can only win, from each player, what it matches:
/// the rest of a bigger bet is a side pot or is returned uncalled. Earlier streets are fully
/// matched by anyone who still has chips, so only this street's commitments are layered.
struct CallTerms {
    double pot{0.0};
    double cost{0.0};
};

CallTerms call_terms(const PokerGameState& state, std::size_t hero, int to_call) {
    const Player& h = state.players[hero];
    if (to_call <= h.stack) {
        return {static_cast<double>(state.pot), static_cast<double>(to_call)};
    }
    const int cost = h.stack;
    std::vector<double> committed;
    committed.reserve(state.players.size());
    double street = 0.0;
    for (const Player& p : state.players) {
        committed.push_back(static_cast<double>(p.committed_this_street));
        street += static_cast<double>(p.committed_this_street);
    }
    committed[hero] += static_cast<double>(cost);
    double eligible = 0.0;
    for (const Side_pot_layer& layer : side_pot_ladder_from_commitments(committed)) {
        if (layer.player_cap_contribution[hero] > 0.0) {
            eligible += layer.pot_chips;
        }
    }
    const double earlier = static_cast<double>(state.pot) - street;
    return {earlier + eligible - static_cast<double>(cost), static_cast<double>(cost)};
}

/// `raise_by` (chips above the call) for a raise of about `fraction` of the pot, kept inside the
/// legal range: at least the minimum raise, at most all-in (which may be a short raise).
int raise_by_for(const PokerGameState& state, const LegalActions& legal, float fraction) {
    const double frac = std::isfinite(fraction) && fraction > 0.0F ? static_cast<double>(fraction) : 0.0;
    const double min_by = static_cast<double>(legal.min_raise_to) - state.current_bet;
    const double max_by = static_cast<double>(legal.max_raise_to) - state.current_bet;
    const double wanted = std::max(min_by, std::round(static_cast<double>(state.pot) * frac));
    return static_cast<int>(std::clamp(wanted, min_by, max_by));
}

}  // namespace

int hero_player_index(const PokerGameState& state, int hero_seat) {
    if (hero_seat >= 0) {
        return player_index_for_seat(state, hero_seat);
    }
    if (state.acting_index >= 0 && state.acting_index < static_cast<int>(state.players.size())) {
        return state.acting_index;
    }
    return state.players.empty() ? -1 : 0;
}

Decision decide_action(const PokerGameState& game_state, const std::vector<Card>& player_hand,
                       const BotConfig& cfg, const OpponentModel* opponent_model, int hero_seat,
                       const CancelPredicate* should_cancel) {
    throw_if_cancelled(should_cancel);
    if (cfg.monte_carlo_simulations < 1) {
        // There is no cheap equity estimate to fall back on: an encoded hand rank orders hands but
        // is not a probability of winning.
        throw std::invalid_argument("monteCarloSimulations must be at least 1");
    }
    const int hero = hero_player_index(game_state, hero_seat);
    if (hero < 0) {
        throw std::invalid_argument("heroSeat " + std::to_string(hero_seat) + " is not at the table");
    }
    if (const char* why = unavailable_actor_reason(game_state, hero)) {
        throw std::invalid_argument(std::string("hero has no action to take: ") + why);
    }
    const LegalActions legal = legal_actions_for(game_state, hero);
    const Player& me = game_state.players[static_cast<std::size_t>(hero)];

    Decision out{};
    std::mt19937 rng(cfg.rng_seed ^ (static_cast<std::uint32_t>(game_state.pot) * 1315423911U));
    double equity = static_cast<double>(simulate_hand_outcome(
        player_hand, game_state.community_cards, cfg.monte_carlo_simulations, rng,
        cfg.monte_carlo_villains, should_cancel));

    if (opponent_model) {
        // Heuristic nudge, not a range model: +/- weight/2 equity for a maximally aggressive or
        // passive opponent (`aggression_factor` in [0, 1], 0.5 neutral).
        const double agg_adj =
            (opponent_model->aggression_factor - 0.5) * cfg.opponent_aggression_weight;
        equity = std::clamp(equity + agg_adj, 0.0, 1.0);
    }

    const auto raise_or = [&](Action fallback) {
        if (!legal.can_raise) {
            out.action = fallback;
            return out;
        }
        out.action = Action::Raise;
        out.raise_by = raise_by_for(game_state, legal, cfg.raise_pot_fraction);
        return out;
    };

    if (legal.to_call == 0) {
        const double raise_threshold =
            std::clamp(static_cast<double>(cfg.aggression_threshold), 0.0, 1.0);
        if (equity >= raise_threshold) {
            return raise_or(Action::Check);
        }
        out.action = Action::Check;
        return out;
    }

    const CallTerms call = call_terms(game_state, static_cast<std::size_t>(hero), legal.to_call);
    const double po = pot_odds_ratio(call.pot, call.cost);
    const double ev_call_now = expected_value_call(equity, call.pot, call.cost);
    const double risk = std::clamp(static_cast<double>(cfg.risk_tolerance), 0.05, 0.95);
    if (ev_call_now < 0.0) {
        out.action = Action::Fold;
        return out;
    }

    if (equity + 1e-9 < po * risk) {
        out.action = Action::Fold;
        return out;
    }

    if (equity < po * (risk + 0.1)) {
        out.action = Action::Call;
        return out;
    }

    if (equity >= static_cast<double>(cfg.aggression_threshold) && me.stack > legal.to_call) {
        return raise_or(Action::Call);
    }

    out.action = Action::Call;
    return out;
}

}  // namespace poker
