#include "poker/strategy.hpp"

#include "poker/hand_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace poker {

namespace {

int resolve_hero_seat(const PokerGameState& state, int hero_seat) {
    if (hero_seat >= 0) {
        return hero_seat;
    }
    if (state.acting_index >= 0 &&
        state.acting_index < static_cast<int>(state.players.size())) {
        return state.players[static_cast<std::size_t>(state.acting_index)].seat;
    }
    if (!state.players.empty()) {
        return state.players[0].seat;
    }
    return 0;
}

double equity_estimate(const PokerGameState& state, const std::vector<Card>& hole,
                       const BotConfig& cfg, std::mt19937& rng) {
    if (cfg.monte_carlo_simulations <= 0) {
        const std::uint64_t s =
            evaluate_hand_strength(hole, state.community_cards);
        return std::min(1.0, static_cast<double>(s) / static_cast<double>(1ULL << 40));
    }
    return static_cast<double>(
        simulate_hand_outcome(hole, state.community_cards, cfg.monte_carlo_simulations, rng,
                              cfg.monte_carlo_villains));
}

}  // namespace

Decision decide_action(const PokerGameState& game_state, const std::vector<Card>& player_hand,
                       const BotConfig& cfg, const OpponentModel* opponent_model,
                       int hero_seat) {
    Decision out{};
    const int seat = resolve_hero_seat(game_state, hero_seat);
    BotView view = make_bot_view(game_state, seat);

    std::mt19937 rng(cfg.rng_seed ^ static_cast<std::uint32_t>(game_state.pot * 1315423911));
    double equity = equity_estimate(game_state, player_hand, cfg, rng);

    if (opponent_model) {
        const double agg_adj =
            (opponent_model->aggression_factor - 0.5) * cfg.opponent_aggression_weight;
        equity = std::clamp(equity + agg_adj, 0.0, 1.0);
    }

    const double po = pot_odds_ratio(view.pot, view.to_call);
    const double ev_call = expected_value_call(equity, view.pot, view.to_call);
    const double risk = std::clamp(static_cast<double>(cfg.risk_tolerance), 0.05, 0.95);

    if (view.to_call == 0) {
        const double raise_threshold =
            std::clamp(static_cast<double>(cfg.aggression_threshold), 0.0, 1.0);
        if (equity >= raise_threshold) {
            out.action = Action::Raise;
            const int bb = std::max(1, view.big_blind);
            out.raise_by = std::max(bb, static_cast<int>(std::round(view.pot * cfg.raise_pot_fraction)));
            return out;
        }
        out.action = Action::Check;
        return out;
    }

    const double ev_call_now = expected_value_call(equity, view.pot, view.to_call);
    if (ev_call_now < 0.0) {
        out.action = Action::Fold;
        return out;
    }

    if (equity + 1e-9 < po * risk) {
        out.action = Action::Fold;
        return out;
    }

    if (ev_call_now >= 0.0 && equity < po * (risk + 0.1)) {
        out.action = Action::Call;
        return out;
    }

    if (equity >= static_cast<double>(cfg.aggression_threshold) && view.hero_stack > view.to_call) {
        out.action = Action::Raise;
        const int bb = std::max(1, view.big_blind);
        out.raise_by = std::max(bb, static_cast<int>(std::round(view.pot * cfg.raise_pot_fraction)));
        return out;
    }

    out.action = Action::Call;
    return out;
}

}  // namespace poker
