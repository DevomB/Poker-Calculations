#include "binding_cfr_subgame.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "poker/cfr_subgame.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/range.hpp"

#include <cstring>
#include <string>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

bool parse_sparse_range(const Napi::Env, const Napi::Value& v, const poker::DeckBitset& dead,
                        poker::SparseRange& out, std::string* err) {
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_float64_array || ta.ElementLength() != 1326) {
            if (err) {
                *err = "dense range must be Float64Array of length 1326";
            }
            return false;
        }
        const double* data = static_cast<const double*>(ta.ArrayBuffer().Data());
        out = poker::sparse_range_from_dense1326(data, 1326, dead.mask);
        return true;
    }
    if (!v.IsObject()) {
        if (err) {
            *err = "range must be Float64Array(1326) or { indices, weights }";
        }
        return false;
    }
    const Napi::Object o = v.As<Napi::Object>();
    std::vector<int> indices;
    std::vector<double> weights;
    const Napi::Value iv = o.Get("indices");
    if (iv.IsTypedArray()) {
        const Napi::TypedArray ta = iv.As<Napi::TypedArray>();
        const std::size_t n = ta.ElementLength();
        indices.resize(n);
        if (ta.TypedArrayType() == napi_int32_array) {
            std::memcpy(indices.data(), ta.ArrayBuffer().Data(), n * sizeof(int32_t));
        } else if (ta.TypedArrayType() == napi_uint32_array) {
            const auto* src = static_cast<const std::uint32_t*>(ta.ArrayBuffer().Data());
            for (std::size_t i = 0; i < n; ++i) {
                indices[i] = static_cast<int>(src[i]);
            }
        } else {
            if (err) {
                *err = "indices must be Int32Array or Uint32Array";
            }
            return false;
        }
    } else if (iv.IsArray()) {
        const Napi::Array a = iv.As<Napi::Array>();
        indices.resize(a.Length());
        for (uint32_t i = 0; i < a.Length(); ++i) {
            indices[i] = a.Get(i).As<Napi::Number>().Int32Value();
        }
    } else {
        if (err) {
            *err = "sparse range needs indices array";
        }
        return false;
    }
    if (!read_f64_vector(o.Get("weights"), "weights", weights, err)) {
        return false;
    }
    out = poker::sparse_range_from_arrays(indices, weights, dead.mask);
    return true;
}

bool parse_mix(const Napi::Value& v, std::vector<double>& out, std::string* err) {
    if (v.IsNumber()) {
        out.assign(1, v.As<Napi::Number>().DoubleValue());
        return true;
    }
    if (!read_f64_vector(v, "mix", out, err)) {
        return false;
    }
    if (out.size() != 1 && out.size() != 1326) {
        if (err) {
            *err = "mix must be a scalar or length-1326 array";
        }
        return false;
    }
    return true;
}

Napi::Object river_solve_to_js(Napi::Env env, const poker::RiverCfrResult& r) {
    Napi::Object out = Napi::Object::New(env);
    out.Set("betFreq", Napi::Number::New(env, r.bet_freq));
    out.Set("callFreq", Napi::Number::New(env, r.call_freq));
    out.Set("evBettor", Napi::Number::New(env, r.ev_bettor));
    out.Set("evDefender", Napi::Number::New(env, r.ev_defender));
    out.Set("iterations", Napi::Number::New(env, r.iterations));
    out.Set("betMix", write_f64_vector(env, r.bet_mix_1326, poker_bind::F64ReturnFormat::Float64));
    out.Set("callMix", write_f64_vector(env, r.call_mix_1326, poker_bind::F64ReturnFormat::Float64));
    return out;
}

bool parse_river_args(const Napi::CallbackInfo& info, double& pot, double& bet, poker::SparseRange& hero,
                      poker::SparseRange& villain, std::vector<poker::Card>& board, int& iterations,
                      std::string* err) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5) {
        if (err) {
            *err = "need pot, betSize, heroRange, villainRange, board[, iterations]";
        }
        return false;
    }
    pot = info[0].As<Napi::Number>().DoubleValue();
    bet = info[1].As<Napi::Number>().DoubleValue();
    board = parse_cards_from_js(env, info[4], err);
    if (err && !err->empty()) {
        return false;
    }
    poker::DeckBitset dead;
    dead.mark_cards(board);
    if (!parse_sparse_range(env, info[2], dead, hero, err)) {
        return false;
    }
    if (!parse_sparse_range(env, info[3], dead, villain, err)) {
        return false;
    }
    iterations = info.Length() >= 6 && info[5].IsNumber() ? info[5].As<Napi::Number>().Int32Value() : 400;
    return true;
}

}  // namespace

Napi::Value RegretMatchingStrategy(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "regretMatchingStrategy(regrets)");
    std::string err;
    std::vector<double> regrets;
    if (!read_f64_vector(info[0], "regrets", regrets, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return write_f64_vector(env, poker::regret_matching_strategy(regrets),
                                poker_bind::F64ReturnFormat::Float64);
    });
}

Napi::Value CfrRiverBetCallFoldSolve(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "cfrRiverBetCallFoldSolve(pot, betSize, heroRange, villainRange, board[, iterations])");
    std::string err;
    double pot = 0;
    double bet = 0;
    int iterations = 400;
    poker::SparseRange hero;
    poker::SparseRange villain;
    std::vector<poker::Card> board;
    if (!parse_river_args(info, pot, bet, hero, villain, board, iterations, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "cfrRiverBetCallFoldSolve args" : err);
    }
    POKER_TRY(env, { return river_solve_to_js(env, poker::cfr_river_bet_call_fold_solve(pot, bet, hero, villain,
                                                                                      board, iterations)); });
}

Napi::Value FictitiousPlayRiver(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "fictitiousPlayRiver(pot, betSize, heroRange, villainRange, board[, iterations])");
    std::string err;
    double pot = 0;
    double bet = 0;
    int iterations = 400;
    poker::SparseRange hero;
    poker::SparseRange villain;
    std::vector<poker::Card> board;
    if (!parse_river_args(info, pot, bet, hero, villain, board, iterations, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "fictitiousPlayRiver args" : err);
    }
    POKER_TRY(env, {
        return river_solve_to_js(env, poker::fictitious_play_river(pot, bet, hero, villain, board, iterations));
    });
}

Napi::Value BestResponseRiver(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 6,
                  "bestResponseRiver(pot, betSize, heroRange, villainRange, board, villainBetMix)");
    std::string err;
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double bet = info[1].As<Napi::Number>().DoubleValue();
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[4], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::DeckBitset dead;
    dead.mark_cards(board);
    poker::SparseRange hero;
    poker::SparseRange villain;
    if (!parse_sparse_range(env, info[2], dead, hero, &err) ||
        !parse_sparse_range(env, info[3], dead, villain, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> mix;
    if (!parse_mix(info[5], mix, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::best_response_river(pot, bet, hero, villain, board, mix);
        Napi::Object out = Napi::Object::New(env);
        out.Set("value", Napi::Number::New(env, r.value));
        out.Set("callFrequency", Napi::Number::New(env, r.call_frequency));
        out.Set("action", Napi::String::New(env, r.action));
        return out;
    });
}

Napi::Value ExploitabilityRiver(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 7,
                  "exploitabilityRiver(pot, betSize, heroRange, villainRange, board, bettorMix, callerMix)");
    std::string err;
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double bet = info[1].As<Napi::Number>().DoubleValue();
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[4], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::DeckBitset dead;
    dead.mark_cards(board);
    poker::SparseRange hero;
    poker::SparseRange villain;
    if (!parse_sparse_range(env, info[2], dead, hero, &err) ||
        !parse_sparse_range(env, info[3], dead, villain, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> bet_mix;
    std::vector<double> call_mix;
    if (!parse_mix(info[5], bet_mix, &err) || !parse_mix(info[6], call_mix, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::exploitability_river(pot, bet, hero, villain, board, bet_mix,
                                                                 call_mix));
    });
}

Napi::Value EvOfStrategyProfile(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 7,
                  "evOfStrategyProfile(pot, betSize, heroRange, villainRange, board, bettorMix, callerMix)");
    std::string err;
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double bet = info[1].As<Napi::Number>().DoubleValue();
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[4], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::DeckBitset dead;
    dead.mark_cards(board);
    poker::SparseRange hero;
    poker::SparseRange villain;
    if (!parse_sparse_range(env, info[2], dead, hero, &err) ||
        !parse_sparse_range(env, info[3], dead, villain, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> bet_mix;
    std::vector<double> call_mix;
    if (!parse_mix(info[5], bet_mix, &err) || !parse_mix(info[6], call_mix, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::ev_of_strategy_profile(pot, bet, hero, villain, board, bet_mix, call_mix);
        Napi::Object out = Napi::Object::New(env);
        out.Set("evBettor", Napi::Number::New(env, r.ev_bettor));
        out.Set("evDefender", Napi::Number::New(env, r.ev_defender));
        return out;
    });
}

Napi::Value CfrHeadsUpPushFoldSolve(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "cfrHeadsUpPushFoldSolve(jammerRange, callerRange, stackBb[, iterations])");
    std::string err;
    poker::DeckBitset dead;
    poker::SparseRange jammer;
    poker::SparseRange caller;
    if (!parse_sparse_range(env, info[0], dead, jammer, &err) ||
        !parse_sparse_range(env, info[1], dead, caller, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double stack = info[2].As<Napi::Number>().DoubleValue();
    const int iterations = info.Length() >= 4 && info[3].IsNumber() ? info[3].As<Napi::Number>().Int32Value() : 400;
    POKER_TRY(env, {
        const auto r = poker::cfr_heads_up_push_fold_solve(jammer, caller, stack, iterations);
        Napi::Object out = Napi::Object::New(env);
        out.Set("jamFreq", Napi::Number::New(env, r.jam_freq));
        out.Set("callFreq", Napi::Number::New(env, r.call_freq));
        out.Set("evJammer", Napi::Number::New(env, r.ev_jammer));
        out.Set("evCaller", Napi::Number::New(env, r.ev_caller));
        out.Set("iterations", Napi::Number::New(env, r.iterations));
        out.Set("jamMix", write_f64_vector(env, r.jam_mix_1326, poker_bind::F64ReturnFormat::Float64));
        out.Set("callMix", write_f64_vector(env, r.call_mix_1326, poker_bind::F64ReturnFormat::Float64));
        return out;
    });
}

Napi::Value StrategySupportSize(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "strategySupportSize(actionProbs[, eps])");
    std::string err;
    std::vector<double> probs;
    if (!read_f64_vector(info[0], "actionProbs", probs, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double eps = info.Length() >= 2 && info[1].IsNumber() ? info[1].As<Napi::Number>().DoubleValue() : 0.02;
    POKER_TRY(env, {
        const auto r = poker::strategy_support_size(probs, eps);
        Napi::Object out = Napi::Object::New(env);
        out.Set("mixedCount", Napi::Number::New(env, r.mixed_count));
        out.Set("pureMass", Napi::Number::New(env, r.pure_mass));
        return out;
    });
}

Napi::Value CfrNodeReachUpdate(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "cfrNodeReachUpdate(cumulativeRegrets, instantaneousRegrets[, reach])");
    std::string err;
    std::vector<double> cum;
    std::vector<double> inst;
    if (!read_f64_vector(info[0], "cumulativeRegrets", cum, &err) ||
        !read_f64_vector(info[1], "instantaneousRegrets", inst, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double reach = info.Length() >= 3 && info[2].IsNumber() ? info[2].As<Napi::Number>().DoubleValue() : 1.0;
    POKER_TRY(env, {
        const auto r = poker::cfr_node_reach_update(cum, inst, reach);
        Napi::Object out = Napi::Object::New(env);
        out.Set("regrets", write_f64_vector(env, r.regrets, poker_bind::F64ReturnFormat::Float64));
        out.Set("strategy", write_f64_vector(env, r.strategy, poker_bind::F64ReturnFormat::Float64));
        return out;
    });
}

Napi::Value SolveHuRiverCheckBetTree(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "solveHuRiverCheckBetTree(pot, betSize, heroRange, villainRange, board[, iterations[, topK]])");
    std::string err;
    double pot = 0;
    double bet = 0;
    int iterations = 400;
    poker::SparseRange hero;
    poker::SparseRange villain;
    std::vector<poker::Card> board;
    if (!parse_river_args(info, pot, bet, hero, villain, board, iterations, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "solveHuRiverCheckBetTree args" : err);
    }
    const int top_k = info.Length() >= 7 && info[6].IsNumber() ? info[6].As<Napi::Number>().Int32Value() : 8;
    POKER_TRY(env, {
        const auto r =
            poker::solve_hu_river_check_bet_tree(pot, bet, hero, villain, board, iterations, top_k);
        Napi::Object out = river_solve_to_js(env, r.solve);
        Napi::Object cls = Napi::Object::New(env);
        cls.Set("airBet", Napi::Number::New(env, r.classes.air_bet));
        cls.Set("drawBet", Napi::Number::New(env, r.classes.draw_bet));
        cls.Set("madeBet", Napi::Number::New(env, r.classes.made_bet));
        cls.Set("strongBet", Napi::Number::New(env, r.classes.strong_bet));
        cls.Set("airCall", Napi::Number::New(env, r.classes.air_call));
        cls.Set("drawCall", Napi::Number::New(env, r.classes.draw_call));
        cls.Set("madeCall", Napi::Number::New(env, r.classes.made_call));
        cls.Set("strongCall", Napi::Number::New(env, r.classes.strong_call));
        out.Set("classes", cls);
        Napi::Array top = Napi::Array::New(env, r.top_bet_combos.size());
        for (std::size_t i = 0; i < r.top_bet_combos.size(); ++i) {
            const auto& row = r.top_bet_combos[i];
            Napi::Object o = Napi::Object::New(env);
            o.Set("comboIndex", Napi::Number::New(env, row.combo_index));
            o.Set("cardA", Napi::Number::New(env, row.card_a));
            o.Set("cardB", Napi::Number::New(env, row.card_b));
            o.Set("betFrequency", Napi::Number::New(env, row.bet_frequency));
            o.Set("weight", Napi::Number::New(env, row.weight));
            top.Set(static_cast<uint32_t>(i), o);
        }
        out.Set("topBetCombos", top);
        return out;
    });
}
