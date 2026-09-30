#include "binding_stud_razz.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "poker/stud_razz.hpp"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using poker_bind::eval_to_object;
using poker_bind::parse_cards_from_js;
using poker_bind::try_parse_cards_from_js;

namespace {

poker_bind::EvalObjectFormat parse_eval_format(const Napi::CallbackInfo& info, std::size_t idx) {
    poker_bind::EvalObjectFormat fmt = poker_bind::EvalObjectFormat::Full;
    if (info.Length() <= idx || !info[idx].IsObject()) {
        return fmt;
    }
    const Napi::Object opts = info[idx].As<Napi::Object>();
    if (!opts.Has("format")) {
        return fmt;
    }
    const Napi::Value fv = opts.Get("format");
    if (!fv.IsString()) {
        throw std::invalid_argument("format must be a string");
    }
    const std::string fs = fv.As<Napi::String>().Utf8Value();
    if (fs == "slim") {
        return poker_bind::EvalObjectFormat::Slim;
    }
    if (fs != "full") {
        throw std::invalid_argument("format must be 'full' or 'slim'");
    }
    return fmt;
}

std::vector<poker::Card> parse_optional_cards(const Napi::CallbackInfo& info, std::size_t idx, std::string* err) {
    if (info.Length() <= idx || info[idx].IsUndefined() || info[idx].IsNull()) {
        return {};
    }
    return parse_cards_from_js(info.Env(), info[idx], err);
}

Napi::Object razz_to_object(Napi::Env env, const poker::RazzEvaluation& e, poker_bind::EvalObjectFormat fmt) {
    poker::HandEvaluation he{};
    he.rank = e.rank;
    he.kickers = e.kickers;
    Napi::Object o = eval_to_object(env, he, fmt);
    o.Set("strength", Napi::Number::New(env, static_cast<double>(poker::pack_razz_strength(e))));
    return o;
}

}  // namespace

Napi::Value EvaluateStudBestHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "evaluateStudBestHand(cards: CardInput, options?)");
    std::vector<poker::Card> cards;
    if (!try_parse_cards_from_js(env, info[0], cards)) {
        return env.Null();
    }
    POKER_TRY(env, {
        const auto fmt = parse_eval_format(info, 1);
        return eval_to_object(env, poker::evaluate_stud_best_hand(cards), fmt);
    });
}

Napi::Value EvaluateRazzHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "evaluateRazzHand(cards: CardInput, options?)");
    std::vector<poker::Card> cards;
    if (!try_parse_cards_from_js(env, info[0], cards)) {
        return env.Null();
    }
    POKER_TRY(env, {
        const auto fmt = parse_eval_format(info, 1);
        return razz_to_object(env, poker::evaluate_razz_hand(cards), fmt);
    });
}

Napi::Value RazzWheelIsNuts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "razzWheelIsNuts(cards: CardInput)");
    std::string err;
    const std::vector<poker::Card> cards = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::razz_wheel_is_nuts(cards)); });
}

Napi::Value ExactHuStudEquityVsKnown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHuStudEquityVsKnown(heroCards, villainCards, extraDead?)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> villain = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> extra = parse_optional_cards(info, 2, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::exact_hu_stud_equity_vs_known(hero, villain, extra));
    });
}

Napi::Value ExactHuRazzEquityVsKnown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHuRazzEquityVsKnown(heroCards, villainCards, extraDead?)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> villain = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> extra = parse_optional_cards(info, 2, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::exact_hu_razz_equity_vs_known(hero, villain, extra));
    });
}
Napi::Value StudDeadCardDeck(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "studDeadCardDeck(deadCards: CardInput)");
    std::string err;
    const std::vector<poker::Card> dead = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const std::vector<std::string> live = poker::stud_dead_card_deck(dead);
        Napi::Array out = Napi::Array::New(env, live.size());
        for (std::size_t i = 0; i < live.size(); ++i) {
            out[static_cast<uint32_t>(i)] = Napi::String::New(env, live[i]);
        }
        return out;
    });
}

Napi::Value SimulateStudEquityVsRandom(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3 && info[1].IsNumber() && info[2].IsNumber(),
                  "simulateStudEquityVsRandom(heroCards, trials, seed, extraDead?)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int trials = info[1].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[2].As<Napi::Number>().Uint32Value());
    const std::vector<poker::Card> extra = parse_optional_cards(info, 3, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::simulate_stud_equity_vs_random(hero, trials, rng, extra));
    });
}

Napi::Value SimulateRazzEquityVsRandom(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3 && info[1].IsNumber() && info[2].IsNumber(),
                  "simulateRazzEquityVsRandom(heroCards, trials, seed, extraDead?)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int trials = info[1].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[2].As<Napi::Number>().Uint32Value());
    const std::vector<poker::Card> extra = parse_optional_cards(info, 3, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::simulate_razz_equity_vs_random(hero, trials, rng, extra));
    });
}
