#include "binding_deuce_seven.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"

#include "poker/deuce_seven.hpp"

#include <random>
#include <string>
#include <vector>

using poker_bind::is_card_input;
using poker_bind::parse_cards_from_js;
using poker_bind::try_parse_cards_from_js;

namespace {

Napi::Array cards_to_js(Napi::Env env, const std::vector<poker::Card>& cards) {
    Napi::Array a = Napi::Array::New(env, cards.size());
    for (std::size_t i = 0; i < cards.size(); ++i) {
        a[static_cast<uint32_t>(i)] = Napi::String::New(env, cards[i].to_string());
    }
    return a;
}

bool parse_eight_pat(const Napi::CallbackInfo& info, std::size_t idx, bool* out) {
    *out = true;
    if (info.Length() <= idx || info[idx].IsUndefined() || info[idx].IsNull()) {
        return true;
    }
    if (info[idx].IsBoolean()) {
        *out = info[idx].As<Napi::Boolean>().Value();
        return true;
    }
    if (info[idx].IsObject()) {
        const Napi::Object o = info[idx].As<Napi::Object>();
        if (o.Has("eightPat") && !o.Get("eightPat").IsUndefined()) {
            if (!o.Get("eightPat").IsBoolean()) {
                return false;
            }
            *out = o.Get("eightPat").As<Napi::Boolean>().Value();
        }
        return true;
    }
    return false;
}

bool parse_side_spec(const Napi::Object& o, const char* discard_key, const char* keep_key,
                     poker::DeuceSevenDrawSpec& spec, std::string* err) {
    const bool has_d = o.Has(discard_key) && !o.Get(discard_key).IsUndefined() && !o.Get(discard_key).IsNull();
    const bool has_k = o.Has(keep_key) && !o.Get(keep_key).IsUndefined() && !o.Get(keep_key).IsNull();
    if (has_d && has_k) {
        if (err) {
            *err = "give discard or keep, not both";
        }
        return false;
    }
    if (has_d) {
        const Napi::Value v = o.Get(discard_key);
        if (v.IsNumber()) {
            spec.has_count = true;
            spec.discard_count = v.As<Napi::Number>().Int32Value();
            return true;
        }
        spec.discard_cards = parse_cards_from_js(o.Env(), v, err);
        if (err && !err->empty()) {
            return false;
        }
        spec.has_discard_cards = true;
        return true;
    }
    if (has_k) {
        spec.keep_cards = parse_cards_from_js(o.Env(), o.Get(keep_key), err);
        if (err && !err->empty()) {
            return false;
        }
        spec.has_keep_cards = true;
        return true;
    }
    return true;
}

std::vector<poker::Card> parse_optional_cards(const Napi::CallbackInfo& info, std::size_t idx, std::string* err) {
    if (info.Length() <= idx || info[idx].IsUndefined() || info[idx].IsNull()) {
        return {};
    }
    return parse_cards_from_js(info.Env(), info[idx], err);
}

}  // namespace

Napi::Value EvaluateDeuceSevenHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "evaluateDeuceSevenHand(cards: CardInput)");
    std::vector<poker::Card> five;
    if (!try_parse_cards_from_js(env, info[0], five)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, static_cast<double>(poker::evaluate_deuce_seven_hand(five)));
    });
}

Napi::Value EvaluateDeuceSevenCategory(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "evaluateDeuceSevenCategory(cards: CardInput)");
    std::vector<poker::Card> five;
    if (!try_parse_cards_from_js(env, info[0], five)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::String::New(env, poker::evaluate_deuce_seven_category(five)); });
}

Napi::Value DeuceSevenIsPat(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "deuceSevenIsPat(cards: CardInput, eightPat?: boolean)");
    std::vector<poker::Card> five;
    if (!try_parse_cards_from_js(env, info[0], five)) {
        return env.Null();
    }
    bool eight_pat = true;
    if (!parse_eight_pat(info, 1, &eight_pat)) {
        POKER_FAIL_TYPE(env, "deuceSevenIsPat: eightPat must be boolean");
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::deuce_seven_is_pat(five, eight_pat)); });
}
Napi::Value DeuceSevenDrawEquityVsKnown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "deuceSevenDrawEquityVsKnown(hero, villain, options?)");
    std::vector<poker::Card> hero;
    std::vector<poker::Card> villain;
    if (!try_parse_cards_from_js(env, info[0], hero) || !try_parse_cards_from_js(env, info[1], villain)) {
        return env.Null();
    }
    poker::DeuceSevenDrawSpec hs;
    poker::DeuceSevenDrawSpec vs;
    std::vector<poker::Card> extra;
    int trials = 400;
    std::uint32_t seed = 1;
    if (info.Length() >= 3 && info[2].IsObject()) {
        const Napi::Object o = info[2].As<Napi::Object>();
        std::string err;
        if (!parse_side_spec(o, "heroDiscard", "heroKeep", hs, &err) ||
            !parse_side_spec(o, "villainDiscard", "villainKeep", vs, &err)) {
            POKER_FAIL_TYPE(env, err.empty() ? "deuceSevenDrawEquityVsKnown: bad draw spec" : err);
        }
        if (o.Has("extraDead") && !o.Get("extraDead").IsUndefined() && !o.Get("extraDead").IsNull()) {
            extra = parse_cards_from_js(env, o.Get("extraDead"), &err);
            if (!err.empty()) {
                POKER_FAIL_TYPE(env, err);
            }
        }
        if (o.Has("trials") && o.Get("trials").IsNumber()) {
            trials = o.Get("trials").As<Napi::Number>().Int32Value();
        }
        if (o.Has("seed") && o.Get("seed").IsNumber()) {
            seed = o.Get("seed").As<Napi::Number>().Uint32Value();
        }
    }
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(
            env, poker::deuce_seven_draw_equity_vs_known(hero, villain, hs, vs, extra, trials, rng));
    });
}
Napi::Value DeuceSevenNutsPat(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "deuceSevenNutsPat(cards: CardInput)");
    std::vector<poker::Card> five;
    if (!try_parse_cards_from_js(env, info[0], five)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::deuce_seven_nuts_pat(five)); });
}

Napi::Value DeuceSevenRoughVsSmooth(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "deuceSevenRoughVsSmooth(a: CardInput, b: CardInput)");
    std::vector<poker::Card> a;
    std::vector<poker::Card> b;
    if (!try_parse_cards_from_js(env, info[0], a) || !try_parse_cards_from_js(env, info[1], b)) {
        return env.Null();
    }
    POKER_TRY(env, {
        const poker::DeuceSevenRoughSmooth r = poker::deuce_seven_rough_vs_smooth(a, b);
        Napi::Object o = Napi::Object::New(env);
        o.Set("cmp", Napi::Number::New(env, r.cmp));
        o.Set("aSmooth", Napi::Boolean::New(env, r.a_smooth));
        o.Set("bSmooth", Napi::Boolean::New(env, r.b_smooth));
        return o;
    });
}

Napi::Value DeuceSevenMultiwayShowdown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1 && info[0].IsArray(),
                  "deuceSevenMultiwayShowdown(hands: CardInput[])");
    const Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<std::vector<poker::Card>> hands;
    hands.reserve(arr.Length());
    std::string err;
    for (uint32_t i = 0; i < arr.Length(); ++i) {
        hands.push_back(parse_cards_from_js(env, arr.Get(i), &err));
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    POKER_TRY(env, {
        const std::vector<double> eq = poker::deuce_seven_multiway_showdown(hands);
        Napi::Array out = Napi::Array::New(env, eq.size());
        for (std::size_t i = 0; i < eq.size(); ++i) {
            out[static_cast<uint32_t>(i)] = Napi::Number::New(env, eq[i]);
        }
        return out;
    });
}
