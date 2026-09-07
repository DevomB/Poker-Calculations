#include "binding_hand_potential.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/deck_bitset.hpp"
#include "poker/hand_potential.hpp"
#include "poker/range.hpp"

#include <cstdint>
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
        const auto* data = static_cast<const double*>(ta.ArrayBuffer().Data());
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
            std::memcpy(indices.data(), ta.ArrayBuffer().Data(), n * sizeof(int32_t));
        } else if (ta.TypedArrayType() == napi_uint32_array) {
            const auto* src = static_cast<const uint32_t*>(ta.ArrayBuffer().Data());
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

poker::DeckBitset dead_from_cards(const std::vector<poker::Card>& a,
                                  const std::vector<poker::Card>& b) {
    poker::DeckBitset dead;
    dead.mark_cards(a);
    dead.mark_cards(b);
    return dead;
}

bool parse_hero_board_range(const Napi::CallbackInfo& info, const char* sig,
                            std::vector<poker::Card>& hero, std::vector<poker::Card>& board,
                            poker::SparseRange& range) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) {
        (void)poker_bind::fail_type(env, sig);
        return false;
    }
    std::string err;
    hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        (void)poker_bind::fail_type(env, err);
        return false;
    }
    board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        (void)poker_bind::fail_type(env, err);
        return false;
    }
    if (!parse_sparse_range(env, info[2], dead_from_cards(hero, board), range, &err)) {
        (void)poker_bind::fail_type(env, err);
        return false;
    }
    return true;
}

Napi::Object breakdown_to_js(Napi::Env env, const poker::HandPotentialBreakdown& r) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("hs", Napi::Number::New(env, r.hs));
    o.Set("ppot", Napi::Number::New(env, r.ppot));
    o.Set("npot", Napi::Number::New(env, r.npot));
    o.Set("ehs", Napi::Number::New(env, r.ehs));
    o.Set("ehs2", Napi::Number::New(env, r.ehs2));
    o.Set("nBehind", Napi::Number::New(env, r.n_behind));
    o.Set("nAhead", Napi::Number::New(env, r.n_ahead));
    o.Set("nTied", Napi::Number::New(env, r.n_tied));
    return o;
}

}  // namespace

Napi::Value HandStrengthVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "handStrengthVsRange(heroHole, board, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Number::New(env, poker::hand_strength_vs_range(hero, board, range)); });
}

Napi::Value PositivePotentialVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "positivePotentialVsRange(heroHole, board, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::positive_potential_vs_range(hero, board, range));
    });
}

Napi::Value NegativePotentialVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "negativePotentialVsRange(heroHole, board, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::negative_potential_vs_range(hero, board, range));
    });
}

Napi::Value EffectiveHandStrength(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "effectiveHandStrength(heroHole, board, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Number::New(env, poker::effective_hand_strength(hero, board, range)); });
}

Napi::Value EffectiveHandStrengthSquared(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "effectiveHandStrengthSquared(heroHole, board, range)", hero,
                                board, range)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::effective_hand_strength_squared(hero, board, range));
    });
}

Napi::Value HandPotentialBreakdownBind(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "handPotentialBreakdown(heroHole, board, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, { return breakdown_to_js(env, poker::hand_potential_breakdown(hero, board, range)); });
}

Napi::Value TwoStreetPositivePotential(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "twoStreetPositivePotential(heroHole, flop, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::two_street_positive_potential(hero, board, range));
    });
}

Napi::Value TwoStreetNegativePotential(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
    poker::SparseRange range;
    if (!parse_hero_board_range(info, "twoStreetNegativePotential(heroHole, flop, range)", hero, board,
                                range)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::two_street_negative_potential(hero, board, range));
    });
}

Napi::Value EquityBucketFromEhs(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2 && info[0].IsNumber() && info[1].IsNumber(),
                  "equityBucketFromEhs(ehs, k)");
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::equity_bucket_from_ehs(info[0].As<Napi::Number>().DoubleValue(),
                                                                    info[1].As<Napi::Number>().Int32Value()));
    });
}

Napi::Value ComboEhsTableVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "comboEhsTableVsRange(board, range[, { trials, seed }])");
    std::string err;
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    if (!parse_sparse_range(env, info[1], dead_from_cards(board, {}), range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::ComboEhsTableOptions opt{};
    if (info.Length() >= 3 && info[2].IsObject() && !info[2].IsTypedArray()) {
        const Napi::Object o = info[2].As<Napi::Object>();
        if (o.Has("trials") && o.Get("trials").IsNumber()) {
            const int t = o.Get("trials").As<Napi::Number>().Int32Value();
            opt.trials = t < 0 ? 0 : static_cast<std::size_t>(t);
        }
        if (o.Has("seed") && o.Get("seed").IsNumber()) {
            opt.seed = o.Get("seed").As<Napi::Number>().Uint32Value();
        }
    }
    POKER_TRY(env, {
        const std::vector<double> table = poker::combo_ehs_table_vs_range(board, range, opt);
        return write_f64_vector(env, table, poker_bind::F64ReturnFormat::Float64);
    });
}

// Register uses HandPotentialBreakdown as the JS export name.
Napi::Value HandPotentialBreakdown(const Napi::CallbackInfo& info) {
    return HandPotentialBreakdownBind(info);
}
