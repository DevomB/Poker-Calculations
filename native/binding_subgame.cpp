#include "binding_subgame.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "poker/combinatorics_exact.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/range.hpp"
#include "poker/range_inference.hpp"
#include "poker/subgame_solvers.hpp"

#include <cstring>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

bool parse_sparse_range(const Napi::Env env, const Napi::Value& v, const poker::DeckBitset& dead,
                        poker::SparseRange& out, std::string* err);

bool parse_sparse_range(const Napi::Env env, const Napi::Value& v, const poker::DeckBitset& dead,
                        poker::SparseRange& out, std::string* err) {
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_float64_array || ta.ElementLength() != 1326) {
            if (err) {
                *err = "dense range must be Float64Array of length 1326";
            }
            return false;
        }
        std::vector<double> w(1326);
        std::memcpy(w.data(), ta.ArrayBuffer().Data(), 1326 * sizeof(double));
        out = poker::sparse_range_from_dense1326(w.data(), 1326, dead.mask);
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
        } else {
            if (err) {
                *err = "indices must be Int32Array";
            }
            return false;
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

Napi::Object materialized_to_js(Napi::Env env, const poker::MaterializedRangeResult& r) {
    Napi::Object out = Napi::Object::New(env);
    Napi::Float64Array w = Napi::Float64Array::New(env, 1326);
    for (std::size_t i = 0; i < 1326; ++i) {
        w.Set(i, r.weights[i]);
    }
    out.Set("weights1326", w);
    out.Set("liveComboCount", Napi::Number::New(env, r.live_combo_count));
    out.Set("weightSum", Napi::Number::New(env, r.weight_sum));
    out.Set("shannonEntropy", Napi::Number::New(env, r.shannon_entropy));
    return out;
}

}  // namespace

Napi::Value MaterializeVillainRangeAfterBlockers(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "materializeVillainRangeAfterBlockers(range, heroHoleCards, boardCards[, knownDead])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead_extra;
    if (info.Length() >= 4 && !info[3].IsUndefined() && !info[3].IsNull()) {
        dead_extra = parse_cards_from_js(env, info[3], &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    POKER_TRY(env, {
        if (info[0].IsTypedArray()) {
            const Napi::TypedArray ta = info[0].As<Napi::TypedArray>();
            const double* data = static_cast<const double*>(ta.ArrayBuffer().Data());
            return materialized_to_js(
                env, poker::materialize_villain_range_after_blockers(data, 1326, hero, board, dead_extra));
        }
        poker::SparseRange prior;
        poker::DeckBitset dead;
        dead.mark_cards(hero);
        dead.mark_cards(board);
        dead.mark_cards(dead_extra);
        if (!parse_sparse_range(env, info[0], dead, prior, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        return materialized_to_js(env, poker::materialize_villain_range_after_blockers_sparse(
                                            prior, hero, board, dead_extra));
    });
}

Napi::Value BayesianRangeUpdateFromAction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "bayesianRangeUpdateFromAction(range, heroHoleCards, boardCards, action, alpha)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[1], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::string action = info[3].As<Napi::String>().Utf8Value();
    const double alpha = info[4].As<Napi::Number>().DoubleValue();
    poker::BayesianActionKind kind = poker::BayesianActionKind::Call;
    if (action == "fold") {
        kind = poker::BayesianActionKind::Fold;
    } else if (action == "raise") {
        kind = poker::BayesianActionKind::Raise;
    } else if (action != "call") {
        POKER_FAIL_TYPE(env, "action must be fold, call, or raise");
    }
    poker::SparseRange prior;
    poker::DeckBitset dead;
    dead.mark_cards(hero);
    dead.mark_cards(board);
    if (!parse_sparse_range(env, info[0], dead, prior, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return materialized_to_js(env,
                                  poker::bayesian_range_update_from_action(prior, hero, board, kind, alpha));
    });
}

Napi::Value SolveRiverPolarizedIndifferenceBet(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "solveRiverPolarizedIndifferenceBet(potBeforeBet, numValueCombos, numBluffCombos[, mdf])");
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double nv = info[1].As<Napi::Number>().DoubleValue();
    const double nb = info[2].As<Napi::Number>().DoubleValue();
    const double mdf = info.Length() >= 4 ? info[3].As<Napi::Number>().DoubleValue() : -1.0;
    POKER_TRY(env, {
        const auto r = poker::solve_river_polarized_indifference_bet(pot, nv, nb, mdf);
        Napi::Object out = Napi::Object::New(env);
        out.Set("betSize", Napi::Number::New(env, r.bet_size));
        out.Set("bluffFrequency", Napi::Number::New(env, r.bluff_frequency));
        out.Set("defenderMdf", Napi::Number::New(env, r.defender_mdf));
        out.Set("evAtIndifference", Napi::Number::New(env, r.ev_at_indifference));
        return out;
    });
}

Napi::Value SolveStageMinimaxRegretBet(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "solveStageMinimaxRegretBet(potBeforeBet, betSizes[], villainFoldFreq, villainCallFreq, heroEquityWhenCalled)");
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    std::string err;
    std::vector<double> bets;
    if (!read_f64_vector(info[1], "betSizes", bets, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double fold_f = info[2].As<Napi::Number>().DoubleValue();
    const double call_f = info[3].As<Napi::Number>().DoubleValue();
    const double eq = info[4].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r =
            poker::solve_stage_minimax_regret_bet(pot, bets, fold_f, call_f, eq);
        Napi::Object out = Napi::Object::New(env);
        out.Set("bestBet", Napi::Number::New(env, r.best_bet));
        out.Set("minimaxRegret", Napi::Number::New(env, r.minimax_regret));
        out.Set("evByAction", write_f64_vector(env, r.ev_by_action, poker_bind::F64ReturnFormat::Array));
        return out;
    });
}

Napi::Value ExactInformationRegretVsClairvoyant(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "exactInformationRegretVsClairvoyant(heroHoleCards, boardCards, range, potBeforeCall, toCall)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    poker::DeckBitset dead;
    dead.mark_cards(hero);
    dead.mark_cards(board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double pot = info[3].As<Napi::Number>().DoubleValue();
    const double to_call = info[4].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::exact_information_regret_vs_clairvoyant(hero, board, range, pot,
                                                                                     to_call));
    });
}

Napi::Value MultiwayEquityIndependenceGap(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "multiwayEquityIndependenceGap(heroHoleCards, boardCards, numSimulations, seed, villains)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int sims = info[2].As<Napi::Number>().Int32Value();
    const int seed = info[3].As<Napi::Number>().Int32Value();
    const int villains = info[4].As<Napi::Number>().Int32Value();
    POKER_TRY(env, {
        const auto r = poker::multiway_equity_independence_gap(hero, board, sims, seed, villains);
        Napi::Object out = Napi::Object::New(env);
        out.Set("exact", Napi::Number::New(env, r.exact));
        out.Set("independentApprox", Napi::Number::New(env, r.independent_approx));
        out.Set("gap", Napi::Number::New(env, r.gap));
        out.Set("villains", Napi::Number::New(env, r.villains));
        return out;
    });
}

Napi::Value SolveSymmetricPushFoldThreshold(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "solveSymmetricPushFoldThreshold(effectiveStack, smallBlind, bigBlind, antePerPlayer)");
    const double stack = info[0].As<Napi::Number>().DoubleValue();
    const double sb = info[1].As<Napi::Number>().DoubleValue();
    const double bb = info[2].As<Napi::Number>().DoubleValue();
    const double ante = info[3].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r = poker::solve_symmetric_push_fold_threshold(stack, sb, bb, ante);
        Napi::Object out = Napi::Object::New(env);
        out.Set("thresholdEquity", Napi::Number::New(env, r.threshold_equity));
        out.Set("jamEvAtThreshold", Napi::Number::New(env, r.jam_ev_at_threshold));
        return out;
    });
}
