#include "binding_suit_isomorphism.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "poker/suit_isomorphism.hpp"

#include <string>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::write_f64_vector;

namespace {

Napi::Array cards_to_js(Napi::Env env, const std::vector<poker::Card>& cards) {
    Napi::Array arr = Napi::Array::New(env, static_cast<uint32_t>(cards.size()));
    for (uint32_t i = 0; i < cards.size(); ++i) {
        arr[i] = Napi::String::New(env, cards[i].to_string());
    }
    return arr;
}

Napi::Array perm_to_js(Napi::Env env, const poker::SuitPerm& perm) {
    Napi::Array arr = Napi::Array::New(env, poker::kSuitCount);
    for (uint32_t i = 0; i < static_cast<uint32_t>(poker::kSuitCount); ++i) {
        arr[i] = Napi::Number::New(env, perm[i]);
    }
    return arr;
}

bool parse_suit_perm(const Napi::Value& v, poker::SuitPerm& out, std::string* err) {
    if (!v.IsArray()) {
        if (err) {
            *err = "suit perm must be an array of 4 integers (new suit for c,d,h,s)";
        }
        return false;
    }
    const Napi::Array a = v.As<Napi::Array>();
    if (a.Length() != 4) {
        if (err) {
            *err = "suit perm must have length 4";
        }
        return false;
    }
    for (uint32_t i = 0; i < 4; ++i) {
        const Napi::Value e = a.Get(i);
        if (!e.IsNumber()) {
            if (err) {
                *err = "suit perm entries must be integers 0..3";
            }
            return false;
        }
        out[i] = e.As<Napi::Number>().Int32Value();
    }
    if (!poker::is_valid_suit_perm(out)) {
        if (err) {
            *err = "suit perm must be a bijection of 0..3";
        }
        return false;
    }
    return true;
}

bool parse_dense_range1326(const Napi::Value& v, std::vector<double>& out, std::string* err) {
    if (!v.IsTypedArray()) {
        if (err) {
            *err = "dense range must be Float64Array of length 1326";
        }
        return false;
    }
    const Napi::TypedArray ta = v.As<Napi::TypedArray>();
    if (ta.TypedArrayType() != napi_float64_array || ta.ElementLength() != 1326) {
        if (err) {
            *err = "dense range must be Float64Array of length 1326";
        }
        return false;
    }
    const Napi::Float64Array fa = v.As<Napi::Float64Array>();
    out.assign(fa.Data(), fa.Data() + 1326);
    return true;
}

}  // namespace

Napi::Value CanonicalFlopBoard(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "canonicalFlopBoard(flop)");
    std::string err;
    const auto flop = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return cards_to_js(env, poker::canonical_flop_board(flop)); });
}

Napi::Value CanonicalBoard(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "canonicalBoard(board)");
    std::string err;
    const auto board = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return cards_to_js(env, poker::canonical_board(board)); });
}

Napi::Value CanonicalHolesAndBoard(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "canonicalHolesAndBoard(holes, board)");
    std::string err;
    const auto holes = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::canonical_holes_and_board(holes, board);
        Napi::Object o = Napi::Object::New(env);
        o.Set("holes", cards_to_js(env, r.holes));
        o.Set("board", cards_to_js(env, r.board));
        o.Set("suitPerm", perm_to_js(env, r.perm));
        return o;
    });
}

Napi::Value SuitPermFromCanonicalFlop(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "suitPermFromCanonicalFlop(flop)");
    std::string err;
    const auto flop = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return perm_to_js(env, poker::suit_perm_from_canonical_flop(flop)); });
}

Napi::Value ApplySuitPermToCards(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "applySuitPermToCards(cards, perm)");
    std::string err;
    const auto cards = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SuitPerm perm{};
    if (!parse_suit_perm(info[1], perm, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return cards_to_js(env, poker::apply_suit_perm_to_cards(cards, perm)); });
}

Napi::Value ApplySuitPermToRange1326(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "applySuitPermToRange1326(range, perm)");
    std::string err;
    std::vector<double> weights;
    if (!parse_dense_range1326(info[0], weights, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SuitPerm perm{};
    if (!parse_suit_perm(info[1], perm, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto out = poker::apply_suit_perm_to_range1326(weights.data(), weights.size(), perm);
        return write_f64_vector(env, out, poker_bind::F64ReturnFormat::Float64);
    });
}

Napi::Value IsomorphicFlopOrbitSize(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "isomorphicFlopOrbitSize(flop)");
    std::string err;
    const auto flop = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return Napi::Number::New(env, poker::isomorphic_flop_orbit_size(flop)); });
}

Napi::Value CountCanonicalFlops(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    (void)info;
    POKER_TRY(env, { return Napi::Number::New(env, poker::count_canonical_flops()); });
}

Napi::Value IsomorphicFlopIndex(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "isomorphicFlopIndex(flop)");
    std::string err;
    const auto flop = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return Napi::Number::New(env, poker::isomorphic_flop_index(flop)); });
}

Napi::Value FlopIndexToCanonical(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1 && info[0].IsNumber(), "flopIndexToCanonical(index)");
    const int index = info[0].As<Napi::Number>().Int32Value();
    POKER_TRY(env, { return cards_to_js(env, poker::flop_index_to_canonical(index)); });
}
