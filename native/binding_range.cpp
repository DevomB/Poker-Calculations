#include "binding_range.hpp"

#include "binding_batch.hpp"
#include "binding_common.hpp"
#include "binding_cards.hpp"
#include "binding_numeric.hpp"

#include "poker/card_string.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/equity_matrix.hpp"
#include "poker/icm.hpp"
#include "poker/monte_carlo_detail.hpp"
#include "poker/range.hpp"
#include "poker/range_equity.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;

namespace {

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
        std::memcpy(w.data(), poker_bind::typed_array_data(ta), 1326 * sizeof(double));
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
    if (!o.Has("indices") || !o.Has("weights")) {
        if (err) {
            *err = "sparse range object needs indices and weights arrays";
        }
        return false;
    }
    std::vector<int> indices;
    std::vector<double> weights;
    const Napi::Value iv = o.Get("indices");
    const Napi::Value wv = o.Get("weights");
    if (iv.IsTypedArray()) {
        const Napi::TypedArray ta = iv.As<Napi::TypedArray>();
        const std::size_t n = ta.ElementLength();
        indices.resize(n);
        if (ta.TypedArrayType() == napi_int32_array) {
            std::memcpy(indices.data(), poker_bind::typed_array_data(ta), n * sizeof(int32_t));
        } else if (ta.TypedArrayType() == napi_uint32_array) {
            const auto* src = static_cast<const uint32_t*>(poker_bind::typed_array_data(ta));
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
        const Napi::Array arr = iv.As<Napi::Array>();
        indices.reserve(arr.Length());
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            if (!arr.Get(i).IsNumber()) {
                if (err) {
                    *err = "indices must be numbers";
                }
                return false;
            }
            indices.push_back(arr.Get(i).As<Napi::Number>().Int32Value());
        }
    } else {
        if (err) {
            *err = "indices must be array or typed array";
        }
        return false;
    }
    if (!read_f64_vector(wv, "weights", weights, err)) {
        return false;
    }
    out = poker::sparse_range_from_arrays(indices, weights, dead.mask);
    return true;
}

poker::DeckBitset dead_mask_from_hero_board(const std::vector<poker::Card>& hero,
                                            const std::vector<poker::Card>& board) {
    poker::DeckBitset dead;
    dead.mark_cards(hero);
    dead.mark_cards(board);
    return dead;
}

}  // namespace

Napi::Value ExactHuEquityVsKnownHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) {
        POKER_FAIL_TYPE(env,
                        "exactHuEquityVsKnownHand(heroHoleCards, villainHoleCards, boardCards)");
    }
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> villain = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double eq = poker::exact_hu_equity_vs_known_hand(hero, villain, board);
    return Napi::Number::New(env, eq);
}

Napi::Value ExactHuEquityVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) {
        POKER_FAIL_TYPE(env, "exactHuEquityVsRange(heroHoleCards, boardCards, range)");
    }
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    const poker::DeckBitset dead = dead_mask_from_hero_board(hero, board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double eq = poker::exact_hu_equity_vs_range(hero, board, range);
    return Napi::Number::New(env, eq);
}

Napi::Value SimulateEquityVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5 || !info[3].IsNumber() || !info[4].IsNumber()) {
        POKER_FAIL_TYPE(env,
                        "simulateEquityVsRange(heroHoleCards, boardCards, range, numSimulations, seed)");
    }
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    const poker::DeckBitset dead = dead_mask_from_hero_board(hero, board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const int sims = info[3].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[4].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        const float eq = poker::simulate_equity_vs_range(hero, board, range, sims, rng);
        return Napi::Number::New(env, eq);
    });
}

Napi::Value SimulateEquityVsRanges(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    constexpr const char* kSig =
        "simulateEquityVsRanges(heroHoleCards, boardCards, ranges, numSimulations, seed, options?)";
    if (info.Length() < 5 || !info[2].IsArray() || !info[3].IsNumber() || !info[4].IsNumber()) {
        POKER_FAIL_TYPE(env, kSig);
    }
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    if (!poker_bind::try_parse_hole_and_board(env, info, hero, board, kSig)) {
        return env.Null();
    }
    const double sims_d = info[3].As<Napi::Number>().DoubleValue();
    if (!std::isfinite(sims_d) || std::trunc(sims_d) != sims_d || sims_d < 1.0 || sims_d > 2147483647.0) {
        POKER_FAIL_RANGE(env, "numSimulations must be an integer of at least 1");
    }
    const int sims = static_cast<int>(sims_d);
    const std::uint32_t seed = static_cast<std::uint32_t>(info[4].As<Napi::Number>().Uint32Value());
    std::vector<poker::Card> dead_cards;
    std::int64_t max_attempts = 100LL * sims + 10000;
    std::string err;
    if (info.Length() >= 6 && info[5].IsObject()) {
        const Napi::Object opts = info[5].As<Napi::Object>();
        if (opts.Has("deadCards") && !opts.Get("deadCards").IsUndefined()) {
            dead_cards = parse_cards_from_js(env, opts.Get("deadCards"), &err);
            if (!err.empty()) {
                POKER_FAIL_TYPE(env, "deadCards: " + err);
            }
        }
        if (opts.Has("maxAttempts") && !opts.Get("maxAttempts").IsUndefined()) {
            const Napi::Value m = opts.Get("maxAttempts");
            const double md = m.IsNumber() ? m.As<Napi::Number>().DoubleValue() : -1.0;
            if (!std::isfinite(md) || std::trunc(md) != md || md < 1.0 || md > 9007199254740991.0) {
                POKER_FAIL_RANGE(env, "maxAttempts must be a positive integer");
            }
            max_attempts = static_cast<std::int64_t>(md);
        }
    }
    poker::DeckBitset dead = dead_mask_from_hero_board(hero, board);
    dead.mark_cards(dead_cards);
    const Napi::Array arr = info[2].As<Napi::Array>();
    if (arr.Length() < 1 || arr.Length() > 9) {
        POKER_FAIL_RANGE(env, "ranges must have 1 to 9 entries (one per opponent)");
    }
    std::vector<poker::SparseRange> ranges(arr.Length());
    for (uint32_t i = 0; i < arr.Length(); ++i) {
        bool ok = false;
        try {
            ok = parse_sparse_range(env, arr.Get(i), dead, ranges[i], &err);
        } catch (const std::exception& e) {
            err = e.what();  // e.g. every combo is blocked
        }
        if (!ok) {
            POKER_FAIL_TYPE(env, "ranges[" + std::to_string(i) + "]: " + err);
        }
    }
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        const poker::McRangesEquityResult r =
            poker::simulate_equity_vs_ranges(hero, board, ranges, dead_cards, sims, max_attempts, rng);
        Napi::Object o = Napi::Object::New(env);
        o.Set("estimate", r.equity.estimate);
        o.Set("se", r.equity.se);
        o.Set("ciLow", r.equity.ci_low);
        o.Set("ciHigh", r.equity.ci_high);
        o.Set("n", r.equity.n);
        o.Set("attempts", static_cast<double>(r.attempts));
        return o;
    });
}

Napi::Value SimulateHandOutcomeDetailed(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4 || !info[2].IsNumber() || !info[3].IsNumber()) {
        POKER_FAIL_TYPE(env,
                        "simulateHandOutcomeDetailed(holeCards, board, numSimulations, seed, villains?)");
    }
    std::string err;
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!poker_bind::try_parse_hole_and_board(env, info, hole, board,
                                              "simulateHandOutcomeDetailed(holeCards, board, ...)")) {
        return env.Null();
    }
    const int sims = info[2].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
    int villains = 1;
    if (info.Length() >= 5 && info[4].IsNumber()) {
        villains = info[4].As<Napi::Number>().Int32Value();
    }
    std::mt19937 rng(seed);
    const poker::McEquityDetailedResult r =
        poker::simulate_hand_outcome_detailed(hole, board, sims, rng, villains);
    Napi::Object o = Napi::Object::New(env);
    o.Set("estimate", r.estimate);
    o.Set("se", r.se);
    o.Set("ciLow", r.ci_low);
    o.Set("ciHigh", r.ci_high);
    o.Set("n", r.n);
    return o;
}

Napi::Value BuildPreflopEquityMatrix(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker::PreflopMatrixOptions opts{};
    if (info.Length() >= 1 && info[0].IsObject()) {
        const Napi::Object o = info[0].As<Napi::Object>();
        if (o.Has("iterations") && o.Get("iterations").IsNumber()) {
            opts.iterations = o.Get("iterations").As<Napi::Number>().Int32Value();
        }
        if (o.Has("seed") && o.Get("seed").IsNumber()) {
            opts.seed = static_cast<std::uint32_t>(o.Get("seed").As<Napi::Number>().Uint32Value());
        }
        if (o.Has("threads") && o.Get("threads").IsNumber()) {
            opts.num_threads =
                static_cast<std::size_t>(o.Get("threads").As<Napi::Number>().Uint32Value());
        }
    }
    std::vector<double> out;
    poker::build_preflop_equity_matrix(opts, out);
    const std::size_t n = out.size();
    Napi::ArrayBuffer buf = Napi::ArrayBuffer::New(env, n * sizeof(double));
    std::memcpy(buf.Data(), out.data(), n * sizeof(double));
    return Napi::Float64Array::New(env, n, buf, 0);
}

Napi::Value EquityDeltaIfCardRemoved(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4 || !info[3].IsNumber()) {
        POKER_FAIL_TYPE(env,
                        "equityDeltaIfCardRemoved(heroHoleCards, boardCards, range, removedDeckIndex)");
    }
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    const poker::DeckBitset dead = dead_mask_from_hero_board(hero, board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const int removed = info[3].As<Napi::Number>().Int32Value();
    const double d = poker::equity_delta_if_card_removed(hero, board, removed, range);
    return Napi::Number::New(env, d);
}

Napi::Value IcmExpectedPayoutsWeitzman(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 2) {
        POKER_FAIL_TYPE(env, "icmExpectedPayoutsWeitzman(stacks[], payouts[], alpha?)");
    }
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    if (!read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    double alpha = 2.0;
    if (info.Length() >= 3 && info[2].IsNumber()) {
        alpha = info[2].As<Napi::Number>().DoubleValue();
    }
    const auto ev = poker::icm_expected_payouts_weitzman(stacks, payouts, alpha);
    return poker_bind::write_f64_vector(env, ev, poker_bind::parse_return_format(info, 3));
}
