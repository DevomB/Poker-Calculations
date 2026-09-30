#include "binding_big_o.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/big_o.hpp"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using poker_bind::eval_to_object;
using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;
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
        throw std::invalid_argument("evaluateBigOBestHand: format must be a string");
    }
    const std::string fs = fv.As<Napi::String>().Utf8Value();
    if (fs == "slim") {
        return poker_bind::EvalObjectFormat::Slim;
    }
    if (fs != "full") {
        throw std::invalid_argument("evaluateBigOBestHand: format must be 'full' or 'slim'");
    }
    return fmt;
}

bool parse_big_o_range(const Napi::Value& v, std::vector<poker::BigORangeCombo>& out, std::string* err) {
    if (!v.IsObject()) {
        if (err) {
            *err = "big O range must be { packed, weights? }";
        }
        return false;
    }
    const Napi::Object o = v.As<Napi::Object>();
    if (!o.Has("packed")) {
        if (err) {
            *err = "big O range needs packed (5 deck ids per combo)";
        }
        return false;
    }
    std::vector<std::uint8_t> packed;
    const Napi::Value pv = o.Get("packed");
    if (pv.IsTypedArray()) {
        const Napi::TypedArray ta = pv.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_uint8_array) {
            if (err) {
                *err = "packed must be Uint8Array or number[]";
            }
            return false;
        }
        const Napi::Uint8Array ua = pv.As<Napi::Uint8Array>();
        packed.assign(ua.Data(), ua.Data() + ua.ElementLength());
    } else if (pv.IsArray()) {
        const Napi::Array a = pv.As<Napi::Array>();
        packed.resize(a.Length());
        for (uint32_t i = 0; i < a.Length(); ++i) {
            if (!a.Get(i).IsNumber()) {
                if (err) {
                    *err = "packed entries must be numbers 0..51";
                }
                return false;
            }
            const int x = a.Get(i).As<Napi::Number>().Int32Value();
            if (x < 0 || x > 51) {
                if (err) {
                    *err = "packed deck id must be 0..51";
                }
                return false;
            }
            packed[i] = static_cast<std::uint8_t>(x);
        }
    } else {
        if (err) {
            *err = "packed must be Uint8Array or number[]";
        }
        return false;
    }
    if (packed.empty() || packed.size() % 5 != 0) {
        if (err) {
            *err = "packed length must be a positive multiple of 5";
        }
        return false;
    }
    const std::size_t n = packed.size() / 5;
    std::vector<double> weights;
    if (o.Has("weights") && !o.Get("weights").IsUndefined()) {
        if (!read_f64_vector(o.Get("weights"), "weights", weights, err)) {
            return false;
        }
        if (weights.size() != n) {
            if (err) {
                *err = "weights length must match packed combo count";
            }
            return false;
        }
    } else {
        weights.assign(n, 1.0);
    }
    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (int k = 0; k < 5; ++k) {
            const int id = static_cast<int>(packed[i * 5 + static_cast<std::size_t>(k)]);
            if (id < 0 || id > 51) {
                if (err) {
                    *err = "packed deck id must be 0..51";
                }
                return false;
            }
            out[i].cards[k] = id;
        }
        out[i].weight = weights[i];
    }
    return true;
}

std::vector<poker::Card> parse_optional_cards(const Napi::CallbackInfo& info, std::size_t idx,
                                              std::string* err) {
    if (info.Length() <= idx || info[idx].IsUndefined() || info[idx].IsNull()) {
        return {};
    }
    return parse_cards_from_js(info.Env(), info[idx], err);
}

}  // namespace

Napi::Value EvaluateBigOBestHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "evaluateBigOBestHand(holeCards: CardInput, boardCards: CardInput, options?)");
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_cards_from_js(env, info[0], hole) || !try_parse_cards_from_js(env, info[1], board)) {
        return env.Null();
    }
    POKER_TRY(env, {
        const auto fmt = parse_eval_format(info, 2);
        return eval_to_object(env, poker::evaluate_big_o_best_hand(hole, board), fmt);
    });
}

Napi::Value EvaluateBigOHandStrength(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "evaluateBigOHandStrength(holeCards: CardInput, boardCards: CardInput)");
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_cards_from_js(env, info[0], hole) || !try_parse_cards_from_js(env, info[1], board)) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, static_cast<double>(poker::evaluate_big_o_hand_strength(hole, board)));
    });
}

Napi::Value ExactHuBigOEquityVsKnown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactHuBigOEquityVsKnown(heroHoleCards, villainHoleCards, boardCards)");
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
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::exact_hu_big_o_equity_vs_known(hero, villain, board));
    });
}

Napi::Value SimulateBigOEquityVsRandom(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4 && info[2].IsNumber() && info[3].IsNumber(),
                  "simulateBigOEquityVsRandom(heroHoleCards, boardCards, trials, seed)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int trials = info[2].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::simulate_big_o_equity_vs_random(hero, board, trials, rng));
    });
}

Napi::Value SimulateBigOEquityVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5 && info[3].IsNumber() && info[4].IsNumber(),
                  "simulateBigOEquityVsRange(heroHoleCards, boardCards, range, trials, seed)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::BigORangeCombo> range;
    if (!parse_big_o_range(info[2], range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const int trials = info[3].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = static_cast<std::uint32_t>(info[4].As<Napi::Number>().Uint32Value());
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::simulate_big_o_equity_vs_range(hero, board, range, trials, rng));
    });
}

Napi::Value BigOComboCount(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "bigOComboCount(deadCards: CardInput)");
    std::string err;
    const std::vector<poker::Card> dead = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, static_cast<double>(poker::big_o_combo_count(dead)));
    });
}

Napi::Value BigONutsOnBoard(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "bigONutsOnBoard(heroHoleCards, boardCards, extraDead?)");
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
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::big_o_nuts_on_board(hero, board, extra)); });
}
Napi::Value BigOMultiwayEquityMc(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4 && info[0].IsArray() && info[2].IsNumber() && info[3].IsNumber(),
                  "bigOMultiwayEquityMc(holeHands[], boardCards, trials, seed)");
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
        const std::vector<double> eq = poker::big_o_multiway_equity_mc(holes, board, trials, rng);
        Napi::Array out = Napi::Array::New(env, eq.size());
        for (std::size_t i = 0; i < eq.size(); ++i) {
            out[static_cast<uint32_t>(i)] = Napi::Number::New(env, eq[i]);
        }
        return out;
    });
}
