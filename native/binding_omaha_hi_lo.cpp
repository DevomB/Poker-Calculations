#include "binding_omaha_hi_lo.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"

#include "poker/omaha_hi_lo.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

using poker_bind::eval_to_object;
using poker_bind::parse_cards_from_js;
using poker_bind::try_parse_cards_from_js;

namespace {

Napi::Object lo_to_object(Napi::Env env, const poker::OmahaLoHand& lo) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("qualifies", Napi::Boolean::New(env, lo.qualifies));
    Napi::Array ranks = Napi::Array::New(env, 5);
    for (uint32_t i = 0; i < 5; ++i) {
        ranks[i] = Napi::Number::New(env, lo.ranks[i]);
    }
    o.Set("ranks", ranks);
    o.Set("key", Napi::Number::New(env, static_cast<double>(lo.key)));
    return o;
}

Napi::Object equity_to_object(Napi::Env env, const poker::OmahaHiLoEquity& e) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("hiEquity", Napi::Number::New(env, e.hi_equity));
    o.Set("loEquity", Napi::Number::New(env, e.lo_equity));
    o.Set("scoopEquity", Napi::Number::New(env, e.scoop_equity));
    o.Set("quarterRate", Napi::Number::New(env, e.quarter_rate));
    o.Set("potShare", Napi::Number::New(env, e.pot_share));
    return o;
}

std::vector<poker::Card> parse_optional_cards(const Napi::CallbackInfo& info, std::size_t idx,
                                              std::string* err) {
    if (info.Length() <= idx || info[idx].IsUndefined() || info[idx].IsNull()) {
        return {};
    }
    return parse_cards_from_js(info.Env(), info[idx], err);
}

std::vector<poker::Card> parse_villain_or_null(const Napi::Value& v, std::string* err) {
    if (v.IsNull() || v.IsUndefined()) {
        return {};
    }
    return parse_cards_from_js(v.Env(), v, err);
}

}  // namespace

Napi::Value EvaluateOmahaLoHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "evaluateOmahaLoHand(holeCards: CardInput, boardCards: CardInput)");
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_cards_from_js(env, info[0], hole) || !try_parse_cards_from_js(env, info[1], board)) {
        return env.Null();
    }
    POKER_TRY(env, { return lo_to_object(env, poker::evaluate_omaha_lo_hand(hole, board)); });
}

Napi::Value OmahaLoQualifies(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "omahaLoQualifies(holeCards: CardInput, boardCards: CardInput)");
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_cards_from_js(env, info[0], hole) || !try_parse_cards_from_js(env, info[1], board)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::omaha_lo_qualifies(hole, board)); });
}

Napi::Value EvaluateOmahaHiLo(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "evaluateOmahaHiLo(holeCards: CardInput, boardCards: CardInput)");
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_cards_from_js(env, info[0], hole) || !try_parse_cards_from_js(env, info[1], board)) {
        return env.Null();
    }
    POKER_TRY(env, {
        const poker::OmahaHiLoHands r = poker::evaluate_omaha_hi_lo(hole, board);
        Napi::Object o = Napi::Object::New(env);
        o.Set("hi", eval_to_object(env, r.hi));
        o.Set("lo", lo_to_object(env, r.lo));
        return o;
    });
}

Napi::Value ExactHuOmahaHiLoEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactHuOmahaHiLoEquity(heroHoleCards, villainHoleCards, boardCards)");
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
    POKER_TRY(env, { return equity_to_object(env, poker::exact_hu_omaha_hi_lo_equity(hero, villain, board)); });
}

Napi::Value SimulateOmahaHiLoEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5 && info[3].IsNumber() && info[4].IsNumber(),
                  "simulateOmahaHiLoEquity(heroHoleCards, villainHoleCards|null, boardCards, trials, seed)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> villain = parse_villain_or_null(info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int trials = info[3].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[4].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return equity_to_object(env, poker::simulate_omaha_hi_lo_equity(hero, villain, board, trials, rng));
    });
}

Napi::Value OmahaScoopProbabilityMc(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5 && info[3].IsNumber() && info[4].IsNumber(),
                  "omahaScoopProbabilityMc(heroHoleCards, villainHoleCards, boardCards, trials, seed)");
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
    const int trials = info[3].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[4].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::omaha_scoop_probability_mc(hero, villain, board, trials, rng));
    });
}

Napi::Value OmahaQuarterProbabilityMc(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5 && info[3].IsNumber() && info[4].IsNumber(),
                  "omahaQuarterProbabilityMc(heroHoleCards, villainHoleCards, boardCards, trials, seed)");
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
    const int trials = info[3].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[4].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env,
                                 poker::omaha_quarter_probability_mc(hero, villain, board, trials, rng));
    });
}

Napi::Value OmahaLoNutsOnBoard(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "omahaLoNutsOnBoard(heroHoleCards, boardCards, extraDead?)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> extra = parse_optional_cards(info, 2, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::omaha_lo_nuts_on_board(hero, board, extra)); });
}

Napi::Value OmahaHiLoNuttedness(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "omahaHiLoNuttedness(heroHoleCards, boardCards, extraDead?)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> extra = parse_optional_cards(info, 2, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const poker::OmahaHiLoNuttedness r = poker::omaha_hi_lo_nuttedness(hero, board, extra);
        Napi::Object o = Napi::Object::New(env);
        o.Set("hiNuts", Napi::Boolean::New(env, r.hi_nuts));
        o.Set("loNuts", Napi::Boolean::New(env, r.lo_nuts));
        o.Set("scoopNuts", Napi::Boolean::New(env, r.scoop_nuts));
        return o;
    });
}

Napi::Value OmahaHiLoMultiwayMc(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4 && info[0].IsArray() && info[2].IsNumber() && info[3].IsNumber(),
                  "omahaHiLoMultiwayMc(holeHands[], boardCards, trials, seed)");
    std::string err;
    const Napi::Array hands = info[0].As<Napi::Array>();
    std::vector<std::vector<poker::Card>> holes;
    holes.reserve(hands.Length());
    for (uint32_t i = 0; i < hands.Length(); ++i) {
        holes.push_back(parse_cards_from_js(env, hands.Get(i), &err));
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int trials = info[2].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        const std::vector<double> eq = poker::omaha_hi_lo_multiway_mc(holes, board, trials, rng);
        Napi::Array out = Napi::Array::New(env, eq.size());
        for (std::size_t i = 0; i < eq.size(); ++i) {
            out[static_cast<uint32_t>(i)] = Napi::Number::New(env, eq[i]);
        }
        return out;
    });
}
