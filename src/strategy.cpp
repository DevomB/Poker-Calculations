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

/// A call priced pot layer by pot layer. After hero's chips go in, the commitments are split into
/// side-pot layers (side_pot_ladder_from_commitments); hero can win only the layers it contributes
/// to, and in each it competes only with the live (not folded) players who contributed to it. Those
/// eligible sets are nested, so ordering the opponents by how deep they reach lets one simulation
/// price every layer (simulate_nested_showdown_shares).
struct CallLayers {
    double cost{0.0};                 // chips the call puts in (all-in for less when short)
    std::vector<double> chips;        // hero-eligible layers, including hero's call
    std::vector<int> opponents;       // live opponents eligible in each of those layers
    [[nodiscard]] double total() const {
        double t = 0.0;
        for (double c : chips) {
            t += c;
        }
        return t;
    }
};

CallLayers call_layers(const PokerGameState& state, std::size_t hero, int to_call) {
    CallLayers out;
    const std::size_t n = state.players.size();
    const Player& h = state.players[hero];
    out.cost = static_cast<double>(std::min(to_call, h.stack));
    // Whole-hand commitments when the state carries consistent ones; otherwise this street's, with
    // the chips from earlier streets as one layer every live player contested.
    double street = 0.0;
    double hand = 0.0;
    bool totals_ok = true;
    for (const Player& p : state.players) {
        street += static_cast<double>(p.committed_this_street);
        hand += static_cast<double>(p.total_committed_hand);
        totals_ok = totals_ok && p.total_committed_hand >= p.committed_this_street;
    }
    totals_ok = totals_ok && hand == static_cast<double>(state.pot);
    std::vector<double> committed(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const Player& p = state.players[i];
        committed[i] = static_cast<double>(totals_ok ? p.total_committed_hand : p.committed_this_street);
    }
    committed[hero] += out.cost;

    std::vector<int> depth(n, 0);  // number of hero-eligible layers each live opponent reaches
    const double earlier = totals_ok ? 0.0 : std::max(0.0, static_cast<double>(state.pot) - street);
    if (earlier > 0.0) {
        out.chips.push_back(earlier);
        for (std::size_t j = 0; j < n; ++j) {
            depth[j] += j != hero && !state.players[j].folded ? 1 : 0;
        }
    }
    for (const Side_pot_layer& layer : side_pot_ladder_from_commitments(committed)) {
        if (layer.player_cap_contribution[hero] <= 0.0) {
            continue;  // above hero's stack: a side pot hero cannot win, or chips returned uncalled
        }
        out.chips.push_back(layer.pot_chips);
        for (std::size_t j = 0; j < n; ++j) {
            depth[j] += j != hero && !state.players[j].folded && layer.player_cap_contribution[j] > 0.0 ? 1 : 0;
        }
    }
    // Layer L (0-based) is contested by every opponent whose depth exceeds L.
    for (std::size_t l = 0; l < out.chips.size(); ++l) {
        int k = 0;
        for (std::size_t j = 0; j < n; ++j) {
            k += depth[j] > static_cast<int>(l) ? 1 : 0;
        }
        out.opponents.push_back(k);
    }
    return out;
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
    // Heuristic nudge, not a range model: +/- weight/2 equity for a maximally aggressive or
    // passive opponent (`aggression_factor` in [0, 1], 0.5 neutral).
    const double agg_adj =
        opponent_model ? (opponent_model->aggression_factor - 0.5) * cfg.opponent_aggression_weight : 0.0;
    const auto villain_equity = [&]() {
        return static_cast<double>(simulate_hand_outcome(player_hand, game_state.community_cards,
                                                         cfg.monte_carlo_simulations, rng,
                                                         cfg.monte_carlo_villains, should_cancel));
    };

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
        const double equity = std::clamp(villain_equity() + agg_adj, 0.0, 1.0);
        const double raise_threshold =
            std::clamp(static_cast<double>(cfg.aggression_threshold), 0.0, 1.0);
        if (equity >= raise_threshold) {
            return raise_or(Action::Check);
        }
        out.action = Action::Check;
        return out;
    }

    // Expected winnings from each layer hero can win, against that layer's live opponents, minus
    // the call. When every layer has exactly `monte_carlo_villains` opponents this is the plain
    // equity estimate applied to the pot, as before.
    const CallLayers call = call_layers(game_state, static_cast<std::size_t>(hero), legal.to_call);
    const double total = call.total();
    const bool plain = std::all_of(call.opponents.begin(), call.opponents.end(),
                                   [&](int k) { return k == std::max(1, cfg.monte_carlo_villains); });
    double win_share = 0.0;  // expected fraction of `total` hero collects
    if (plain) {
        win_share = villain_equity();
    } else {
        const std::vector<double> shares = simulate_nested_showdown_shares(
            player_hand, game_state.community_cards, call.opponents, cfg.monte_carlo_simulations, rng,
            should_cancel);
        for (std::size_t l = 0; l < shares.size(); ++l) {
            win_share += call.chips[l] * shares[l];
        }
        win_share = total > 0.0 ? win_share / total : 0.0;
    }
    const double equity = std::clamp(win_share + agg_adj, 0.0, 1.0);
    const double pot_before_call = std::max(0.0, total - call.cost);  // package pot convention
    const double po = pot_odds_ratio(pot_before_call, call.cost);
    const double ev_call_now = expected_value_call(equity, pot_before_call, call.cost);
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
