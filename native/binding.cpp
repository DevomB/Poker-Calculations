#include <napi.h>

#include "async_workers.hpp"
#include "binding_batch.hpp"
#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "binding_state.hpp"
#include "poker/card_string.hpp"
#include "poker/game_state.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/opponent_model.hpp"
#include "poker/poker_math.hpp"
#include "poker/icm.hpp"
#include "poker/side_pot.hpp"
#include "poker/exact_equity.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/strategy.hpp"
#include "poker/types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using poker_bind::DecideActionParsed;
using poker_bind::action_name;
using poker_bind::doubles_from_js_array;
using poker_bind::eval_to_object;
using poker_bind::hand_rank_js;
using poker_bind::is_card_input;
using poker_bind::matrix_from_js_array;
using poker_bind::packed_card_bytes;
using poker_bind::parse_cards_from_js;
using poker_bind::parse_decide_action_inputs;
using poker_bind::parse_return_format;
using poker_bind::read_f64_vector;
using poker_bind::strings_from_js_array;
using poker_bind::write_f64_matrix_flat;
using poker_bind::write_f64_vector;
using poker_bind::F64ReturnFormat;

// parse_exact_hu_args, parse_straight_made_args, parse_benchmark_iterations — binding_batch.hpp

Napi::Value EvaluateBestHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1) {
            throw std::invalid_argument("evaluateBestHand(cards: CardInput)");
        }
        std::string err;
        auto cards = parse_cards_from_js(env, info[0], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        if (cards.empty() || cards.size() > 7) {
            throw std::invalid_argument("need 1..7 cards");
        }
        poker_bind::EvalObjectFormat fmt = poker_bind::EvalObjectFormat::Full;
        if (info.Length() >= 2 && info[1].IsObject()) {
            const Napi::Object opts = info[1].As<Napi::Object>();
            if (opts.Has("format")) {
                const Napi::Value fv = opts.Get("format");
                if (fv.IsString()) {
                    const std::string fs = fv.As<Napi::String>().Utf8Value();
                    if (fs == "slim") {
                        fmt = poker_bind::EvalObjectFormat::Slim;
                    } else if (fs != "full") {
                        throw std::invalid_argument("evaluateBestHand: format must be 'full' or 'slim'");
                    }
                } else {
                    throw std::invalid_argument("evaluateBestHand: format must be a string");
                }
            }
        }
        const poker::HandEvaluation e = poker::evaluate_best_hand(cards);
        return eval_to_object(env, e, fmt);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

static void parse_hole_and_board_or_throw(const Napi::CallbackInfo& info, const Napi::Env& env,
                                           std::vector<poker::Card>& hole,
                                           std::vector<poker::Card>& board, const char* signature) {
    if (info.Length() < 2) {
        throw std::invalid_argument(signature);
    }
    std::string err;
    hole = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        throw std::invalid_argument(err);
    }
    board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        throw std::invalid_argument(err);
    }
}

static Napi::Value HandStrengthAsNumber(const Napi::Env& env, std::uint64_t s) {
    return Napi::Number::New(env, static_cast<double>(s));
}

static Napi::Value EvalHandStrength(const Napi::CallbackInfo& info, bool use_fast) {
    const Napi::Env env = info.Env();
    const char* sig = use_fast ? "evaluateHandStrengthFast(holeCards: CardInput, board: CardInput)"
                               : "evaluateHandStrength(holeCards: CardInput, board: CardInput)";
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    parse_hole_and_board_or_throw(info, env, hole, board, sig);
    const std::uint64_t s =
        use_fast ? poker::evaluate_hand_strength_fast(hole, board) : poker::evaluate_hand_strength(hole, board);
    return HandStrengthAsNumber(env, s);
}

Napi::Value EvaluateHandStrength(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        return EvalHandStrength(info, false);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EvaluateHandStrengthFast(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        return EvalHandStrength(info, true);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BenchmarkEvaluatorThroughput(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        std::string err;
        const std::size_t iterations = parse_benchmark_iterations(info, &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const auto bench = poker::benchmark_evaluator_throughput(iterations);
        Napi::Object out = Napi::Object::New(env);
        out.Set("legacyEvalsPerSecond", Napi::Number::New(env, bench.legacy_evals_per_second));
        out.Set("fastEvalsPerSecond", Napi::Number::New(env, bench.fast_evals_per_second));
        out.Set("implementation", Napi::String::New(env, bench.implementation));
        return out;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BenchmarkEvaluatorThroughputAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        std::string err;
        const std::size_t iterations = parse_benchmark_iterations(info, &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const Napi::Value signal = poker_bind::parse_async_signal(info);
        return poker_async::enqueue_benchmark(env, iterations, signal);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EvaluateHandCategory(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2) {
            throw std::invalid_argument("evaluateHandCategory(holeCards: CardInput, board: CardInput)");
        }
        std::string err;
        auto hole = parse_cards_from_js(env, info[0], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        auto board = parse_cards_from_js(env, info[1], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const poker::HandRank r = poker::evaluate_hand(hole, board);
        return Napi::String::New(env, hand_rank_js(r));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value DecideAction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        poker_bind::DecideActionParsed args{};
        std::string err;
        if (!parse_decide_action_inputs(info, args, &err)) {
            throw std::invalid_argument(err.empty() ? "invalid state" : err);
        }
        const poker::OpponentModel* opp_ptr = args.opponent ? &(*args.opponent) : nullptr;
        const poker::Decision d =
            poker::decide_action(args.state, args.hero_hole, args.cfg, opp_ptr, args.hero_seat);

        Napi::Object out = Napi::Object::New(env);
        out.Set("action", Napi::String::New(env, action_name(d.action)));
        out.Set("raiseBy", Napi::Number::New(env, d.raise_by));
        return out;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value DecideActionAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        poker_bind::DecideActionParsed args{};
        std::string err;
        if (!parse_decide_action_inputs(info, args, &err)) {
            throw std::invalid_argument(err.empty() ? "invalid state" : err);
        }
        const Napi::Value signal = poker_bind::parse_async_signal(info);
        return poker_async::enqueue_decide_action(env, std::move(args.state), std::move(args.hero_hole),
                                                  args.cfg, args.opponent, args.hero_seat, signal);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PotOddsRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("potOddsRatio(pot, toCall)");
        }
        const int pot = info[0].As<Napi::Number>().Int32Value();
        const int to_call = info[1].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::pot_odds_ratio(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ExpectedValueCall(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("expectedValueCall(equity, pot, toCall)");
        }
        const double equity = info[0].As<Napi::Number>().DoubleValue();
        const int pot = info[1].As<Napi::Number>().Int32Value();
        const int to_call = info[2].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::expected_value_call(equity, pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value Spr(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("spr(potChips, effectiveStackChips)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double eff = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::spr(pot, eff));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EffectiveStack(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() == 0) {
            return Napi::Number::New(env, poker::effective_stack({}));
        }
        std::vector<double> stacks;
        stacks.reserve(info.Length());
        for (std::size_t i = 0; i < info.Length(); ++i) {
            if (!info[i].IsNumber()) {
                throw std::invalid_argument("effectiveStack(...stackChips): all args must be numbers");
            }
            stacks.push_back(info[i].As<Napi::Number>().DoubleValue());
        }
        return Napi::Number::New(env, poker::effective_stack(stacks));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value NormalizedStackFractions(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 1) {
            throw std::invalid_argument("normalizedStackFractions(stacks[], returnFormat?)");
        }
        std::string err;
        std::vector<double> stacks;
        if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
            throw std::invalid_argument(err);
        }
        const auto fr = poker::normalized_stack_fractions(stacks);
        return write_f64_vector(env, fr, parse_return_format(info, 1));
    });
}

Napi::Value BreakevenCallEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("breakevenCallEquity(potBeforeCall, toCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_call_equity(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value MinimumDefenseFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("minimumDefenseFrequency(potBeforeOpponentBet, opponentBetSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::minimum_defense_frequency(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value StackInBigBlinds(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("stackInBigBlinds(stackChips, bigBlind)");
        }
        const double stack = info[0].As<Napi::Number>().DoubleValue();
        const double bb = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::stack_in_big_blinds(stack, bb));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PotOddsRatioDisplay(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("potOddsRatioDisplay(potBeforeCall, toCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::pot_odds_ratio_display(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FormatPotOdds(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("formatPotOdds(potBeforeCall, toCall, decimals?)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        int decimals = 2;
        if (info.Length() >= 3 && info[2].IsNumber()) {
            decimals = info[2].As<Napi::Number>().Int32Value();
        }
        return Napi::String::New(env, poker::format_pot_odds(pot, to_call, decimals));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenCallEquityFromPotOddsDisplayRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("breakevenCallEquityFromPotOddsDisplayRatio(displayPotToCallRatio)");
        }
        const double r = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_call_equity_from_pot_odds_display_ratio(r));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PotOddsDisplayRatioFromBreakevenCallEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("potOddsDisplayRatioFromBreakevenCallEquity(breakevenEquity)");
        }
        const double e = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::pot_odds_display_ratio_from_breakeven_call_equity(e));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FormatPotOddsReducedFraction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("formatPotOddsReducedFraction(potBeforeCall, toCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        return Napi::String::New(env, poker::format_pot_odds_reduced_fraction(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EquityToWinningOddsAgainst(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("equityToWinningOddsAgainst(equity)");
        }
        const double e = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::equity_to_winning_odds_against(e));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value WinningOddsAgainstToEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("winningOddsAgainstToEquity(oddsAgainst)");
        }
        const double o = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::winning_odds_against_to_equity(o));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RuleOfFourEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("ruleOfFourEquity(outs)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::rule_of_four_equity(outs));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RuleOfTwoEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("ruleOfTwoEquity(outs)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::rule_of_two_equity(outs));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ImpliedBreakevenFutureWin(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("impliedBreakevenFutureWin(potBeforeCall, toCall, equity)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        const double equity = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::implied_breakeven_future_win(pot, to_call, equity));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BluffToValueRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("bluffToValueRatio(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::bluff_to_value_ratio(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ValueToBluffRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("valueToBluffRatio(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::value_to_bluff_ratio(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BetAsPotFraction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("betAsPotFraction(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::bet_as_pot_fraction(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value SprAfterCall(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("sprAfterCall(potBeforeCall, toCall, effectiveStackBeforeCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        const double stack = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::spr_after_call(pot, to_call, stack));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value CommitmentRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("commitmentRatio(toCall, effectiveStackBeforeCall)");
        }
        const double to_call = info[0].As<Napi::Number>().DoubleValue();
        const double stack = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::commitment_ratio(to_call, stack));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value AlphaFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("alphaFrequency(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::alpha_frequency(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquityPureBluff(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("breakevenFoldEquityPureBluff(potBeforeHeroBet, heroBetOrCallSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_fold_equity_pure_bluff(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquitySemiBluff(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "breakevenFoldEquitySemiBluff(potBeforeHeroBet, heroBetSize, equityWhenCalled, "
                "totalPotIfCalled)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double hero_bet = info[1].As<Napi::Number>().DoubleValue();
        const double eq = info[2].As<Napi::Number>().DoubleValue();
        const double total = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env,
                                 poker::breakeven_fold_equity_semi_bluff(pot, hero_bet, eq, total));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value HypergeometricOneCardHitProbability(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("hypergeometricOneCardHitProbability(outs, unseenCards)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        const double unseen = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::hypergeometric_one_card_hit_probability(outs, unseen));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RunnerRunnerBackdoorFlushTwoCardProbability(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument(
                "runnerRunnerBackdoorFlushTwoCardProbability(suitCardsRemaining, unseenCards)");
        }
        const double s = info[0].As<Napi::Number>().DoubleValue();
        const double u = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::runner_runner_flush_two_card_probability(s, u));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FlopToRiverAtLeastOneHitProbability(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("flopToRiverAtLeastOneHitProbability(outs, unseenAfterFlop)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        const double u = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::flop_to_river_at_least_one_hit_probability(outs, u));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FlopToRiverAtLeastOneHitDisjointOutsSum(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsArray()) {
            throw std::invalid_argument(
                "flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop, outsPerCategory[])");
        }
        const double u = info[0].As<Napi::Number>().DoubleValue();
        const std::vector<double> cats =
            doubles_from_js_array(info[1].As<Napi::Array>(), "outsPerCategory");
        return Napi::Number::New(env, poker::flop_to_river_at_least_one_hit_disjoint_outs_sum(u, cats));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ReverseImpliedOddsMaxFutureLoss(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "reverseImpliedOddsMaxFutureLoss(potBeforeCall, toCall, equity)");
        }
        const double p = info[0].As<Napi::Number>().DoubleValue();
        const double tc = info[1].As<Napi::Number>().DoubleValue();
        const double eq = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::reverse_implied_odds_max_future_loss(p, tc, eq));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value GeometricPotAfterMatchedPotFractions(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "geometricPotAfterMatchedPotFractions(pot0, fraction, nRounds)");
        }
        const double pot0 = info[0].As<Napi::Number>().DoubleValue();
        const double frac = info[1].As<Napi::Number>().DoubleValue();
        const int n = info[2].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::geometric_pot_after_matched_pot_fractions(pot0, frac, n));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value HarringtonM(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument("harringtonM(stackChips, smallBlind, bigBlind, totalAntes)");
        }
        const double st = info[0].As<Napi::Number>().DoubleValue();
        const double sb = info[1].As<Napi::Number>().DoubleValue();
        const double bb = info[2].As<Napi::Number>().DoubleValue();
        const double antes = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::harrington_m(st, sb, bb, antes));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value KellyCriterionBinary(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("kellyCriterionBinary(winProbability, netOdds)");
        }
        const double p = info[0].As<Napi::Number>().DoubleValue();
        const double b = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::kelly_criterion_binary(p, b));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value MonteCarloStandardError(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("monteCarloStandardError(pHat, nTrials)");
        }
        const double ph = info[0].As<Napi::Number>().DoubleValue();
        const int n = info[1].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::monte_carlo_standard_error(ph, n));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BetaBinomialFoldPosterior(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "betaBinomialFoldPosterior(priorAlpha, priorBeta, folds, calls)");
        }
        const double a = info[0].As<Napi::Number>().DoubleValue();
        const double b = info[1].As<Napi::Number>().DoubleValue();
        const int f = info[2].As<Napi::Number>().Int32Value();
        const int c = info[3].As<Napi::Number>().Int32Value();
        const auto r = poker::beta_binomial_fold_update(a, b, f, c);
        Napi::Object o = Napi::Object::New(env);
        o.Set("alpha", Napi::Number::New(env, r.alpha));
        o.Set("beta", Napi::Number::New(env, r.beta));
        o.Set("posteriorMean", Napi::Number::New(env, r.posterior_mean));
        return o;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value DuplicationAdjustedOuts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("duplicationAdjustedOuts(outs, numVillains, duplicationWeight)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        const int nv = info[1].As<Napi::Number>().Int32Value();
        const double w = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::duplication_adjusted_outs(outs, nv, w));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RiskOfRuinDiffusionApprox(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "riskOfRuinDiffusionApprox(driftPerHand, variancePerHand, bankroll)");
        }
        const double mu = info[0].As<Napi::Number>().DoubleValue();
        const double var = info[1].As<Napi::Number>().DoubleValue();
        const double b = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::risk_of_ruin_diffusion_approx(mu, var, b));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BankrollForTargetRorDiffusion(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "bankrollForTargetRorDiffusion(driftPerHand, variancePerHand, targetRor)");
        }
        const double mu = info[0].As<Napi::Number>().DoubleValue();
        const double var = info[1].As<Napi::Number>().DoubleValue();
        const double r = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::bankroll_for_target_ror_diffusion(mu, var, r));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value WilsonScoreInterval(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("wilsonScoreInterval(successes, nTrials, z)");
        }
        const int s = info[0].As<Napi::Number>().Int32Value();
        const int n = info[1].As<Napi::Number>().Int32Value();
        const double z = info[2].As<Napi::Number>().DoubleValue();
        const auto w = poker::wilson_score_interval(s, n, z);
        Napi::Object o = Napi::Object::New(env);
        o.Set("lower", Napi::Number::New(env, w.lower));
        o.Set("upper", Napi::Number::New(env, w.upper));
        return o;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value AgrestiCoullInterval(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("agrestiCoullInterval(successes, nTrials, z)");
        }
        const int s = info[0].As<Napi::Number>().Int32Value();
        const int n = info[1].As<Napi::Number>().Int32Value();
        const double z = info[2].As<Napi::Number>().DoubleValue();
        const auto w = poker::agresti_coull_interval(s, n, z);
        Napi::Object o = Napi::Object::New(env);
        o.Set("lower", Napi::Number::New(env, w.lower));
        o.Set("upper", Napi::Number::New(env, w.upper));
        return o;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value NormalWaldBinomialInterval(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("normalWaldBinomialInterval(successes, nTrials, z)");
        }
        const int s = info[0].As<Napi::Number>().Int32Value();
        const int n = info[1].As<Napi::Number>().Int32Value();
        const double z = info[2].As<Napi::Number>().DoubleValue();
        const auto w = poker::normal_wald_binomial_interval(s, n, z);
        Napi::Object o = Napi::Object::New(env);
        o.Set("lower", Napi::Number::New(env, w.lower));
        o.Set("upper", Napi::Number::New(env, w.upper));
        return o;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value MonteCarloTrialsForHoeffdingBound(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("monteCarloTrialsForHoeffdingBound(epsilon, delta)");
        }
        const double eps = info[0].As<Napi::Number>().DoubleValue();
        const double delta = info[1].As<Napi::Number>().DoubleValue();
        const std::int64_t n = poker::monte_carlo_trials_for_hoeffding_bound(eps, delta);
        return Napi::Number::New(env, static_cast<double>(n));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RakeFromPot(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("rakeFromPot(potChips, rakeFraction, rakeCap)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double rf = info[1].As<Napi::Number>().DoubleValue();
        const double cap = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::rake_from_pot(pot, rf, cap));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenCallEquityWithRake(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "breakevenCallEquityWithRake(potBeforeCall, toCall, rakeFraction, rakeCap)");
        }
        const double p = info[0].As<Napi::Number>().DoubleValue();
        const double tc = info[1].As<Napi::Number>().DoubleValue();
        const double rf = info[2].As<Napi::Number>().DoubleValue();
        const double cap = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_call_equity_with_rake(p, tc, rf, cap));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquitySemiBluffWithRake(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 6 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber() || !info[5].IsNumber()) {
            throw std::invalid_argument(
                "breakevenFoldEquitySemiBluffWithRake(potBeforeHeroBet, heroBetSize, "
                "equityWhenCalled, totalPotIfCalled, rakeFraction, rakeCap)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double hb = info[1].As<Napi::Number>().DoubleValue();
        const double eq = info[2].As<Napi::Number>().DoubleValue();
        const double tot = info[3].As<Napi::Number>().DoubleValue();
        const double rf = info[4].As<Napi::Number>().DoubleValue();
        const double cap = info[5].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(
            env, poker::breakeven_fold_equity_semi_bluff_with_rake(pot, hb, eq, tot, rf, cap));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value MultiwaySymmetricBreakevenCallEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "multiwaySymmetricBreakevenCallEquity(potBefore, toCall, symmetricExtraCallers)");
        }
        const double p = info[0].As<Napi::Number>().DoubleValue();
        const double tc = info[1].As<Napi::Number>().DoubleValue();
        const int k = info[2].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::multiway_symmetric_breakeven_call_equity(p, tc, k));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value TwoStreetPureBluffSameFoldEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "twoStreetPureBluffSameFoldEquity(potBeforeStreet1, betStreet1, betStreet2)");
        }
        const double p0 = info[0].As<Napi::Number>().DoubleValue();
        const double b1 = info[1].As<Napi::Number>().DoubleValue();
        const double b2 = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::two_street_pure_bluff_same_fold_equity(p0, b1, b2));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ChubukovSymmetricJamBreakevenStack(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("chubukovSymmetricJamBreakevenStack(deadMoneyChips, equity)");
        }
        const double d = info[0].As<Napi::Number>().DoubleValue();
        const double eq = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::chubukov_symmetric_jam_breakeven_stack(d, eq));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ChubukovSymmetricJamEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("chubukovSymmetricJamEv(jamStackChips, deadMoneyChips, equity)");
        }
        const double s = info[0].As<Napi::Number>().DoubleValue();
        const double d = info[1].As<Napi::Number>().DoubleValue();
        const double eq = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::chubukov_symmetric_jam_ev(s, d, eq));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ChubukovMaxSymmetricJamStackChipsBinarySearch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "chubukovMaxSymmetricJamStackChipsBinarySearch(equity, deadMoneyChips, maxStackChips)");
        }
        const double eq = info[0].As<Napi::Number>().DoubleValue();
        const double dead = info[1].As<Napi::Number>().DoubleValue();
        const int max_s = info[2].As<Napi::Number>().Int32Value();
        const int out = poker::chubukov_max_symmetric_jam_stack_chips_binary_search(eq, dead, max_s);
        return Napi::Number::New(env, out);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value IcmWinProbabilitiesHarville(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 1) {
            throw std::invalid_argument("icmWinProbabilitiesHarville(stacks[], returnFormat?)");
        }
        std::string err;
        std::vector<double> stacks;
        if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
            throw std::invalid_argument(err);
        }
        const auto w = poker::icm_win_probabilities_harville(stacks);
        return write_f64_vector(env, w, parse_return_format(info, 1));
    });
}

Napi::Value IcmExpectedPayouts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 2) {
            throw std::invalid_argument("icmExpectedPayouts(stacks[], payouts[], returnFormat?)");
        }
        std::string err;
        std::vector<double> stacks;
        std::vector<double> pay;
        if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
            throw std::invalid_argument(err);
        }
        if (!read_f64_vector(info[1], "payouts", pay, &err)) {
            throw std::invalid_argument(err);
        }
        const auto ev = poker::icm_expected_payouts(stacks, pay);
        return write_f64_vector(env, ev, parse_return_format(info, 2));
    });
}

Napi::Value IcmPairwiseBubbleFactor(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 5 || !info[0].IsArray() || !info[1].IsArray() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber()) {
            throw std::invalid_argument(
                "icmPairwiseBubbleFactor(stacks[], payouts[], heroIndex, villainIndex, potChips)");
        }
        const std::vector<double> stacks = doubles_from_js_array(info[0].As<Napi::Array>(), "stacks");
        const std::vector<double> pay = doubles_from_js_array(info[1].As<Napi::Array>(), "payouts");
        const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
        const std::size_t vil = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
        const double pot = info[4].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::icm_pairwise_bubble_factor(stacks, pay, hero, vil, pot));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value SidePotLadderFromCommitments(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 1) {
            throw std::invalid_argument("sidePotLadderFromCommitments(committedChips[])");
        }
        std::string err;
        std::vector<double> c;
        if (!read_f64_vector(info[0], "committed", c, &err)) {
            throw std::invalid_argument(err);
        }
        const auto layers = poker::side_pot_ladder_from_commitments(c);
        Napi::Array arr = Napi::Array::New(env, static_cast<uint32_t>(layers.size()));
        for (uint32_t i = 0; i < layers.size(); ++i) {
            const auto& L = layers[static_cast<std::size_t>(i)];
            Napi::Object o = Napi::Object::New(env);
            o.Set("potChips", Napi::Number::New(env, L.pot_chips));
            Napi::Array contrib = Napi::Array::New(env,
                                                    static_cast<uint32_t>(L.player_cap_contribution.size()));
            for (uint32_t j = 0; j < L.player_cap_contribution.size(); ++j) {
                contrib[j] = Napi::Number::New(env, L.player_cap_contribution[static_cast<std::size_t>(j)]);
            }
            o.Set("playerCapContribution", contrib);
            arr[i] = o;
        }
        return arr;
    });
}

Napi::Value LayeredPotChipEvFromEquities(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 2) {
            throw std::invalid_argument(
                "layeredPotChipEvFromEquities(layerPotChips[], equityPlayerByLayer[][], returnFormat?)");
        }
        std::string err;
        std::vector<double> pots;
        std::vector<std::vector<double>> mat;
        if (!read_f64_vector(info[0], "layerPots", pots, &err)) {
            throw std::invalid_argument(err);
        }
        if (info[1].IsTypedArray()) {
            if (info.Length() < 3 || !info[2].IsNumber()) {
                throw std::invalid_argument(
                    "layeredPotChipEvFromEquities: flat equity matrix requires cols argument");
            }
            const int cols = info[2].As<Napi::Number>().Int32Value();
            if (!read_f64_matrix_flat(info[1], cols, "equityMatrix", mat, &err)) {
                throw std::invalid_argument(err);
            }
            const auto ev = poker::layered_pot_chip_ev_from_equities(pots, mat);
            return write_f64_vector(env, ev, parse_return_format(info, 3));
        }
        if (!read_f64_matrix(info[1], "equityMatrix", mat, &err)) {
            throw std::invalid_argument(err);
        }
        const auto ev = poker::layered_pot_chip_ev_from_equities(pots, mat);
        return write_f64_vector(env, ev, parse_return_format(info, 2));
    });
}

Napi::Value SidePotLayersTotalChips(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsArray()) {
            throw std::invalid_argument("sidePotLayersTotalChips(sidePotLayers[])");
        }
        const Napi::Array arr = info[0].As<Napi::Array>();
        std::vector<poker::Side_pot_layer> layers;
        layers.reserve(arr.Length());
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            const Napi::Value v = arr[i];
            if (!v.IsObject()) {
                throw std::invalid_argument("sidePotLayersTotalChips: each layer must be an object");
            }
            const Napi::Object o = v.As<Napi::Object>();
            if (!o.Has("potChips") || !o.Get("potChips").IsNumber()) {
                throw std::invalid_argument("sidePotLayersTotalChips: each layer needs numeric potChips");
            }
            poker::Side_pot_layer L{};
            L.pot_chips = o.Get("potChips").As<Napi::Number>().DoubleValue();
            layers.push_back(std::move(L));
        }
        return Napi::Number::New(env, poker::side_pot_layers_total_chips(layers));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ExactHuEquityVsRandomHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        ExactHuParsed args{};
        std::string err;
        if (!parse_exact_hu_args(info, args, &err)) {
            throw std::invalid_argument(err);
        }
        const double eq = poker::exact_hu_equity_vs_random_hand(args.hero, args.board);
        return Napi::Number::New(env, eq);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ExactHuEquityVsRandomHandAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        ExactHuParsed args{};
        std::string err;
        if (!parse_exact_hu_args(info, args, &err)) {
            throw std::invalid_argument(err);
        }
        const auto hero = std::move(args.hero);
        const auto board = std::move(args.board);
        const Napi::Value signal = poker_bind::parse_async_signal(info);
        return poker_async::enqueue_float_work(
            env,
            [hero, board](const poker::CancelPredicate* cancel) {
                return poker::exact_hu_equity_vs_random_hand(hero, board, cancel);
            },
            signal);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value StraightMadeFlopToRiverExactProbability(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        StraightMadeParsed args{};
        std::string err;
        if (!parse_straight_made_args(info, args, &err)) {
            throw std::invalid_argument(err);
        }
        const double p = poker::straight_made_flop_to_river_exact_probability(args.hero, args.flop, args.dead);
        return Napi::Number::New(env, p);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value StraightMadeFlopToRiverExactProbabilityAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        StraightMadeParsed args{};
        std::string err;
        if (!parse_straight_made_args(info, args, &err)) {
            throw std::invalid_argument(err);
        }
        const auto hero = std::move(args.hero);
        const auto flop = std::move(args.flop);
        const auto dead = std::move(args.dead);
        const Napi::Value signal = poker_bind::parse_async_signal(info);
        return poker_async::enqueue_float_work(
            env,
            [hero, flop, dead](const poker::CancelPredicate* cancel) {
                return poker::straight_made_flop_to_river_exact_probability(hero, flop, dead, cancel);
            },
            signal);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ChubukovMaxSymmetricJamStackBinarySearch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[2].IsNumber() || !info[3].IsNumber()) {
            throw std::invalid_argument(
                "chubukovMaxSymmetricJamStackBinarySearch(heroHoleCards: CardInput, boardCards: CardInput, "
                "deadMoneyChips, maxStackChips)");
        }
        std::string err;
        std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const double dead = info[2].As<Napi::Number>().DoubleValue();
        const double max_d = info[3].As<Napi::Number>().DoubleValue();
        if (!std::isfinite(max_d)) {
            throw std::invalid_argument("maxStackChips must be finite");
        }
        if (max_d < 1.0) {
            return Napi::Number::New(env, 0);
        }
        const double cap = static_cast<double>(std::numeric_limits<int>::max());
        const double clamped = std::min(max_d, cap);
        const int max_s = static_cast<int>(clamped);
        const int s =
            poker::chubukov_max_symmetric_jam_stack_from_hand_binary_search(hero, board, dead, max_s);
        return Napi::Number::New(env, s);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ChubukovMaxSymmetricJamStackFromHandBinarySearch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[2].IsNumber() || !info[3].IsNumber()) {
            throw std::invalid_argument(
                "chubukovMaxSymmetricJamStackFromHandBinarySearch(heroHoleCards: CardInput, boardCards: "
                "CardInput, deadMoneyChips, maxStackChips)");
        }
        std::string err;
        std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const double dead = info[2].As<Napi::Number>().DoubleValue();
        const int max_s = info[3].As<Napi::Number>().Int32Value();
        const int s = poker::chubukov_max_symmetric_jam_stack_from_hand_binary_search(hero, board, dead, max_s);
        return Napi::Number::New(env, s);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value IcmHarvillePlacementProbabilities(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 1) {
            throw std::invalid_argument("icmHarvillePlacementProbabilities(stacks[], returnFormat?)");
        }
        std::string err;
        std::vector<double> stacks;
        if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
            throw std::invalid_argument(err);
        }
        const auto m = poker::icm_harville_placement_probabilities(stacks);
        return write_f64_matrix_flat(env, m, parse_return_format(info, 1));
    });
}

Napi::Value FlopToRiverAtLeastOneHitUnionTwoCategories(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "flopToRiverAtLeastOneHitUnionTwoCategories(unseenAfterFlop, outsA, outsB, sharedAb)");
        }
        const double u = info[0].As<Napi::Number>().DoubleValue();
        const double a = info[1].As<Napi::Number>().DoubleValue();
        const double b = info[2].As<Napi::Number>().DoubleValue();
        const double s = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::flop_to_river_at_least_one_hit_union_two_categories(u, a, b, s));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FlopToRiverAtLeastOneHitUnionThreeCategories(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 8 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber() || !info[5].IsNumber() || !info[6].IsNumber() ||
            !info[7].IsNumber()) {
            throw std::invalid_argument(
                "flopToRiverAtLeastOneHitUnionThreeCategories(unseen, oa, ob, oc, sab, sac, sbc, sabc)");
        }
        const double u = info[0].As<Napi::Number>().DoubleValue();
        const double oa = info[1].As<Napi::Number>().DoubleValue();
        const double ob = info[2].As<Napi::Number>().DoubleValue();
        const double oc = info[3].As<Napi::Number>().DoubleValue();
        const double sab = info[4].As<Napi::Number>().DoubleValue();
        const double sac = info[5].As<Napi::Number>().DoubleValue();
        const double sbc = info[6].As<Napi::Number>().DoubleValue();
        const double sabc = info[7].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(
            env, poker::flop_to_river_at_least_one_hit_union_three_categories(u, oa, ob, oc, sab, sac, sbc,
                                                                                sabc));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FlopToRiverAtLeastOneHitUnionFourCategories(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 16) {
            throw std::invalid_argument(
                "flopToRiverAtLeastOneHitUnionFourCategories(unseen, oa, ob, oc, od, s01, s02, s03, s12, "
                "s13, s23, s012, s013, s023, s123, fourWay)");
        }
        for (std::size_t i = 0; i < 16; ++i) {
            if (!info[i].IsNumber()) {
                throw std::invalid_argument(
                    "flopToRiverAtLeastOneHitUnionFourCategories: all arguments must be numbers");
            }
        }
        const double u = info[0].As<Napi::Number>().DoubleValue();
        const double oa = info[1].As<Napi::Number>().DoubleValue();
        const double ob = info[2].As<Napi::Number>().DoubleValue();
        const double oc = info[3].As<Napi::Number>().DoubleValue();
        const double od = info[4].As<Napi::Number>().DoubleValue();
        const double s01 = info[5].As<Napi::Number>().DoubleValue();
        const double s02 = info[6].As<Napi::Number>().DoubleValue();
        const double s03 = info[7].As<Napi::Number>().DoubleValue();
        const double s12 = info[8].As<Napi::Number>().DoubleValue();
        const double s13 = info[9].As<Napi::Number>().DoubleValue();
        const double s23 = info[10].As<Napi::Number>().DoubleValue();
        const double s012 = info[11].As<Napi::Number>().DoubleValue();
        const double s013 = info[12].As<Napi::Number>().DoubleValue();
        const double s023 = info[13].As<Napi::Number>().DoubleValue();
        const double s123 = info[14].As<Napi::Number>().DoubleValue();
        const double fw = info[15].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::flop_to_river_at_least_one_hit_union_four_categories(
                                         u, oa, ob, oc, od, s01, s02, s03, s12, s13, s23, s012, s013, s023,
                                         s123, fw));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RunnerRunnerStraightDrawHitProbability(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "runnerRunnerStraightDrawHitProbability(straightKind, deadAmongOuts, unseenAfterFlop) "
                "— straightKind: 0=gutshot4, 1=openEnded8, 2=doubleBelly8");
        }
        const int k = info[0].As<Napi::Number>().Int32Value();
        const int dead = info[1].As<Napi::Number>().Int32Value();
        const double u = info[2].As<Napi::Number>().DoubleValue();
        poker::Runner_runner_straight_draw_kind kind{};
        if (k == 0) {
            kind = poker::Runner_runner_straight_draw_kind::GutshotFourOut;
        } else if (k == 1) {
            kind = poker::Runner_runner_straight_draw_kind::OpenEndedEightOut;
        } else if (k == 2) {
            kind = poker::Runner_runner_straight_draw_kind::DoubleBellyBusterEightOut;
        } else {
            throw std::invalid_argument("straightKind must be 0, 1, or 2");
        }
        return Napi::Number::New(env, poker::runner_runner_straight_draw_hit_probability(kind, dead, u));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value HarringtonMEffective(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber()) {
            throw std::invalid_argument(
                "harringtonMEffective(stackChips, smallBlind, bigBlind, antePerActivePlayer, "
                "numActivePlayers)");
        }
        const double st = info[0].As<Napi::Number>().DoubleValue();
        const double sb = info[1].As<Napi::Number>().DoubleValue();
        const double bb = info[2].As<Napi::Number>().DoubleValue();
        const double ap = info[3].As<Napi::Number>().DoubleValue();
        const int n = info[4].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::harrington_m_effective(st, sb, bb, ap, n));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value HarringtonMEffectiveActiveAntes(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "harringtonMEffectiveActiveAntes(stackChips, smallBlind, bigBlind, antesFromActiveSeats[])");
        }
        const double st = info[0].As<Napi::Number>().DoubleValue();
        const double sb = info[1].As<Napi::Number>().DoubleValue();
        const double bb = info[2].As<Napi::Number>().DoubleValue();
        std::string err;
        std::vector<double> antes;
        if (!read_f64_vector(info[3], "antesFromActiveSeats", antes, &err)) {
            throw std::invalid_argument(err);
        }
        return Napi::Number::New(env, poker::harrington_m_effective_active_antes(st, sb, bb, antes));
    });
}

Napi::Value MultiwaySymmetricBreakevenCallEquityWithShare(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber()) {
            throw std::invalid_argument(
                "multiwaySymmetricBreakevenCallEquityWithShare(potBefore, toCall, symmetricExtraCallers, "
                "shareModel, heroFractionWhenWin) — shareModel 0=WTA 1=fixedFraction");
        }
        const double p = info[0].As<Napi::Number>().DoubleValue();
        const double tc = info[1].As<Napi::Number>().DoubleValue();
        const int k = info[2].As<Napi::Number>().Int32Value();
        const int m = info[3].As<Napi::Number>().Int32Value();
        const double frac = info[4].As<Napi::Number>().DoubleValue();
        const auto model = m == 0 ? poker::Multiway_symmetric_pot_share_model::WinnerTakesAll
                                  : poker::Multiway_symmetric_pot_share_model::FixedHeroShareWhenWins;
        return Napi::Number::New(env, poker::multiway_symmetric_breakeven_call_equity_with_share(
                                         p, tc, k, model, frac));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value TwoStreetPureBluffEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber()) {
            throw std::invalid_argument(
                "twoStreetPureBluffEv(potBeforeStreet1, betStreet1, betStreet2, fe1, fe2)");
        }
        const double p0 = info[0].As<Napi::Number>().DoubleValue();
        const double b1 = info[1].As<Napi::Number>().DoubleValue();
        const double b2 = info[2].As<Napi::Number>().DoubleValue();
        const double fe1 = info[3].As<Napi::Number>().DoubleValue();
        const double fe2 = info[4].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::two_street_pure_bluff_ev(p0, b1, b2, fe1, fe2));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquitySecondStreetPureBluff(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "breakevenFoldEquitySecondStreetPureBluff(potBeforeStreet1, betStreet1, betStreet2, fe1)");
        }
        const double p0 = info[0].As<Napi::Number>().DoubleValue();
        const double b1 = info[1].As<Napi::Number>().DoubleValue();
        const double b2 = info[2].As<Napi::Number>().DoubleValue();
        const double fe1 = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(
            env, poker::breakeven_fold_equity_second_street_pure_bluff(p0, b1, b2, fe1));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquityFirstStreetPureBluff(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "breakevenFoldEquityFirstStreetPureBluff(potBeforeStreet1, betStreet1, betStreet2, fe2)");
        }
        const double p0 = info[0].As<Napi::Number>().DoubleValue();
        const double b1 = info[1].As<Napi::Number>().DoubleValue();
        const double b2 = info[2].As<Napi::Number>().DoubleValue();
        const double fe2 = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(
            env, poker::breakeven_fold_equity_first_street_pure_bluff(p0, b1, b2, fe2));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquityPureBluffWithRake(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "breakevenFoldEquityPureBluffWithRake(potBeforeHeroBet, heroBetOrCallSize, rakeFraction, "
                "rakeCap)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        const double rf = info[2].As<Napi::Number>().DoubleValue();
        const double cap = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_fold_equity_pure_bluff_with_rake(pot, bet, rf, cap));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ValidateCardString(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsString()) {
            throw std::invalid_argument("validateCardString(card: string)");
        }
        poker::Card c{};
        std::string perr;
        const bool ok = poker_bind::parse_card_string_from_js(env, info[0].As<Napi::String>(), c, &perr);
        return Napi::Boolean::New(env, ok);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value CardStringsHaveDuplicate(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1) {
            throw std::invalid_argument("cardStringsHaveDuplicate(cards: CardInput)");
        }
        const std::uint8_t* data = nullptr;
        std::size_t len = 0;
        std::string perr;
        if (packed_card_bytes(info[0], &data, &len, &perr)) {
            std::string derr;
            if (!poker::packed_cards_have_duplicate(data, len, &derr)) {
                if (!derr.empty()) {
                    throw std::invalid_argument(derr);
                }
                return Napi::Boolean::New(env, false);
            }
            return Napi::Boolean::New(env, true);
        }
        if (info[0].IsArray()) {
            std::string derr;
            const bool dup = poker_bind::js_card_array_has_duplicate(env, info[0].As<Napi::Array>(), &derr);
            if (!derr.empty()) {
                throw std::invalid_argument(derr);
            }
            return Napi::Boolean::New(env, dup);
        }
        throw std::invalid_argument("cardStringsHaveDuplicate(cards: CardInput)");
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value CanonicalCardString(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsString()) {
            throw std::invalid_argument("canonicalCardString(card: string)");
        }
        const std::string s = poker::canonical_card_string(info[0].As<Napi::String>().Utf8Value());
        return Napi::String::New(env, s);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ParseCompactCardList(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsString()) {
            throw std::invalid_argument("parseCompactCardList(cards: string)");
        }
        bool packed_out = false;
        if (info.Length() >= 2 && info[1].IsObject()) {
            const Napi::Object opts = info[1].As<Napi::Object>();
            if (opts.Has("outFormat")) {
                const Napi::Value of = opts.Get("outFormat");
                if (!of.IsString()) {
                    throw std::invalid_argument("parseCompactCardList: outFormat must be a string");
                }
                const std::string fmt = of.As<Napi::String>().Utf8Value();
                if (fmt == "packed") {
                    packed_out = true;
                } else if (fmt != "strings") {
                    throw std::invalid_argument("parseCompactCardList: outFormat must be 'strings' or 'packed'");
                }
            }
        }
        const std::string text = info[0].As<Napi::String>().Utf8Value();
        if (packed_out) {
            const auto indices = poker::parse_compact_card_list_indices(text);
            Napi::Uint8Array ta = Napi::Uint8Array::New(env, indices.size());
            for (std::size_t i = 0; i < indices.size(); ++i) {
                ta[i] = indices[i];
            }
            return ta;
        }
        const auto cards = poker::parse_compact_card_list(text);
        Napi::Array a = Napi::Array::New(env, static_cast<uint32_t>(cards.size()));
        for (uint32_t i = 0; i < cards.size(); ++i) {
            a[i] = Napi::String::New(env, cards[static_cast<std::size_t>(i)]);
        }
        return a;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EstimatedOutsFromRuleOfTwo(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("estimatedOutsFromRuleOfTwo(equity, unseenCards)");
        }
        const double eq = info[0].As<Napi::Number>().DoubleValue();
        const double u = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::estimated_outs_from_rule_of_two(eq, u));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EstimatedOutsFromRuleOfFour(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("estimatedOutsFromRuleOfFour(equity, unseenCards)");
        }
        const double eq = info[0].As<Napi::Number>().DoubleValue();
        const double u = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::estimated_outs_from_rule_of_four(eq, u));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value MonteCarloTrialsForStandardErrorBound(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("monteCarloTrialsForStandardErrorBound(pHat, targetSe)");
        }
        const double ph = info[0].As<Napi::Number>().DoubleValue();
        const double se = info[1].As<Napi::Number>().DoubleValue();
        const std::int64_t n = poker::monte_carlo_trials_for_standard_error_bound(ph, se);
        return Napi::Number::New(env, static_cast<double>(n));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ExpectedValueCallWithRake(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsNumber()) {
            throw std::invalid_argument(
                "expectedValueCallWithRake(equity, potBeforeCall, toCall, rakeFraction, rakeCap)");
        }
        const double eq = info[0].As<Napi::Number>().DoubleValue();
        const double pot = info[1].As<Napi::Number>().DoubleValue();
        const double tc = info[2].As<Napi::Number>().DoubleValue();
        const double rf = info[3].As<Napi::Number>().DoubleValue();
        const double cap = info[4].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::expected_value_call_with_rake(eq, pot, tc, rf, cap));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PreflopCombosFromNotation(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsString()) {
            throw std::invalid_argument("preflopCombosFromNotation(notation: string)");
        }
        const int n = poker::preflop_combos_from_notation(info[0].As<Napi::String>().Utf8Value());
        return Napi::Number::New(env, n);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PreflopCombosFromNotationsList(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsArray()) {
            throw std::invalid_argument("preflopCombosFromNotationsList(notations: string[])");
        }
        const auto strs = strings_from_js_array(info[0].As<Napi::Array>(), "preflopCombosFromNotationsList");
        const int sum = poker::preflop_combos_from_notations_list(strs);
        return Napi::Number::New(env, sum);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value HandRankCategoryOrder(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsString()) {
            throw std::invalid_argument("handRankCategoryOrder(category: string)");
        }
        const int ord = poker::hand_rank_category_order(info[0].As<Napi::String>().Utf8Value());
        return Napi::Number::New(env, ord);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value NlMinimumRaiseToTotal(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument(
                "nlMinimumRaiseToTotal(currentMaxWager, lastRaiseIncrement, bigBlind)");
        }
        const double cur = info[0].As<Napi::Number>().DoubleValue();
        const double inc = info[1].As<Napi::Number>().DoubleValue();
        const double bb = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::nl_minimum_raise_to_total(cur, inc, bb));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value OrbitCostChips(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsArray()) {
            throw std::invalid_argument("orbitCostChips(smallBlind, bigBlind, antesFromSeats[])");
        }
        const double sb = info[0].As<Napi::Number>().DoubleValue();
        const double bb = info[1].As<Napi::Number>().DoubleValue();
        const std::vector<double> antes = doubles_from_js_array(info[2].As<Napi::Array>(), "antesFromSeats");
        return Napi::Number::New(env, poker::orbit_cost_chips(sb, bb, antes));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value HarringtonQ(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsArray()) {
            throw std::invalid_argument("harringtonQ(heroStack, stacks[])");
        }
        const double hero = info[0].As<Napi::Number>().DoubleValue();
        const std::vector<double> stacks = doubles_from_js_array(info[1].As<Napi::Array>(), "stacks");
        return Napi::Number::New(env, poker::harrington_q(hero, stacks));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value CompareBestHands(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2) {
            throw std::invalid_argument("compareBestHands(cardsA: CardInput, cardsB: CardInput)");
        }
        std::string err;
        auto a = parse_cards_from_js(env, info[0], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        err.clear();
        auto b = parse_cards_from_js(env, info[1], &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const int cmp = poker::compare_best_hands(a, b);
        return Napi::Number::New(env, cmp);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value IcmTopKFinishProbabilities(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 2 || !info[1].IsNumber()) {
            throw std::invalid_argument("icmTopKFinishProbabilities(stacks[], k, returnFormat?)");
        }
        std::string err;
        std::vector<double> stacks;
        if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
            throw std::invalid_argument(err);
        }
        const int k = info[1].As<Napi::Number>().Int32Value();
        const auto probs = poker::icm_top_k_finish_probabilities(stacks, k);
        return write_f64_vector(env, probs, parse_return_format(info, 2));
    });
}

Napi::Value IcmLastPlaceProbabilitiesHarville(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_TRY(env, {
        if (info.Length() < 1) {
            throw std::invalid_argument("icmLastPlaceProbabilitiesHarville(stacks[], returnFormat?)");
        }
        std::string err;
        std::vector<double> stacks;
        if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
            throw std::invalid_argument(err);
        }
        const auto probs = poker::icm_last_place_probabilities_harville(stacks);
        return write_f64_vector(env, probs, parse_return_format(info, 1));
    });
}

