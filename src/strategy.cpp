#include "poker/strategy.hpp"

#include "poker/cancel.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/poker_math.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

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
    throw std::invalid_argument("decideAction requires an acting player or an explicit hero seat");
}

double equity_estimate(const PokerGameState& state, const std::vector<Card>& hole,
                       const BotConfig& cfg, std::mt19937& rng,
                       const CancelPredicate* should_cancel) {
    throw_if_cancelled(should_cancel);
    if (cfg.monte_carlo_simulations <= 0) {
        const std::uint64_t s =
            evaluate_hand_strength_fast(hole, state.community_cards);
        return std::min(1.0, static_cast<double>(s) / static_cast<double>(1ULL << 40));
    }
    return static_cast<double>(simulate_hand_outcome(hole, state.community_cards,
                                                       cfg.monte_carlo_simulations, rng,
                                                       cfg.monte_carlo_villains, should_cancel));
}

}  // namespace

Decision decide_action(const PokerGameState& game_state, const std::vector<Card>& player_hand,
                       const BotConfig& cfg, const OpponentModel* opponent_model, int hero_seat,
                       const CancelPredicate* should_cancel) {
    throw_if_cancelled(should_cancel);
    Decision out{};
    const int seat = resolve_hero_seat(game_state, hero_seat);
    BotView view = make_bot_view(game_state, seat);
    if (game_state.phase == GamePhase::Showdown || game_state.phase == GamePhase::HandComplete ||
        view.hero_stack <= 0 || std::none_of(game_state.players.begin(), game_state.players.end(),
            [seat](const Player& p) { return p.seat == seat && !p.folded; })) {
        throw std::invalid_argument("decideAction requires a live hero with chips in an unfinished hand");
    }

    if (!std::isfinite(cfg.raise_pot_fraction)) {
        throw std::invalid_argument("raisePotFraction must be finite");
    }
    std::mt19937 rng(cfg.rng_seed ^ (static_cast<std::uint32_t>(game_state.pot) * 1315423911U));
    // ponytail: One-street equity thresholds ignore future betting; upgrade to range-conditioned action EVs.
    double equity = equity_estimate(game_state, player_hand, cfg, rng, should_cancel);

    if (opponent_model) {
        const double agg_adj =
            (opponent_model->aggression_factor - 0.5) * cfg.opponent_aggression_weight;
        equity = std::clamp(equity + agg_adj, 0.0, 1.0);
    }

    const double po = pot_odds_ratio(view.pot, view.to_call);
    const double risk = std::clamp(static_cast<double>(cfg.risk_tolerance), 0.05, 0.95);
    if (view.to_call > 0) {
        if (expected_value_call(equity, view.pot, view.to_call) < 0.0 || equity + 1e-9 < po * risk) {
            out.action = Action::Fold;
            return out;
        }
        if (equity < po * (risk + 0.1)) {
            out.action = Action::Call;
            return out;
        }
    }

    const bool can_be_called = std::any_of(game_state.players.begin(), game_state.players.end(),
        [seat](const Player& p) { return p.seat != seat && !p.folded && p.stack > 0; });
    if (equity >= std::clamp(static_cast<double>(cfg.aggression_threshold), 0.0, 1.0) &&
        view.hero_stack > view.to_call && can_be_called) {
        out.action = Action::Raise;
        const int maximum = view.hero_stack - view.to_call;
        // A short all-in may be smaller than a full minimum raise.
        const int minimum = std::min(maximum, std::max({1, view.big_blind, game_state.last_raise_increment}));
        out.raise_by = static_cast<int>(std::clamp(
            std::round(static_cast<double>(view.pot) * cfg.raise_pot_fraction),
            static_cast<double>(minimum), static_cast<double>(maximum)));
        return out;
    }

    out.action = view.to_call == 0 ? Action::Check : Action::Call;
    return out;
}

}  // namespace poker
