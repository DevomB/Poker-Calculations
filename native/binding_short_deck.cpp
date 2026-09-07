#include "binding_short_deck.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_init.hpp"

#include "poker/card_string.hpp"
#include "poker/range.hpp"
#include "poker/short_deck.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using poker_bind::hand_rank_string_interned;
using poker_bind::parse_cards_from_js;
using poker_bind::try_parse_cards_from_js;
using poker_bind::try_parse_hole_and_board;

namespace {

Napi::Object short_eval_to_object(Napi::Env env, const poker::ShortDeckEvaluation& e) {
    Napi::Object o = Napi::Object::New(env);
    const int rank_category = static_cast<int>(e.rank);
    std::uint64_t packed = static_cast<std::uint64_t>(e.rank) << 24;
    for (int i = 0; i < 5; ++i) {
        packed |= static_cast<std::uint64_t>(e.kickers[static_cast<std::size_t>(i)] & 0x1F) << (4 * (4 - i));
    }
    o.Set("rankCategory", Napi::Number::New(env, rank_category));
    o.Set("strength", Napi::Number::New(env, static_cast<double>(packed)));
    o.Set("rank", hand_rank_string_interned(env, poker::short_deck_rank_to_label(e.rank)));
    Napi::Array kickers = Napi::Array::New(env, 5);
    for (std::size_t i = 0; i < e.kickers.size(); ++i) {
        kickers[i] = Napi::Number::New(env, e.kickers[i]);
    }
    o.Set("kickers", kickers);
    return o;
}

bool parse_class_weights(const Napi::Env env, const Napi::Value& v,
                         std::array<double, poker::kShortDeckHandClasses>& out, std::string* err) {
    out.fill(0.0);
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_float64_array) {
            if (err) {
                *err = "range typed array must be Float64Array";
            }
            return false;
        }
        const std::size_t n = ta.ElementLength();
        const auto* src = static_cast<const double*>(ta.ArrayBuffer().Data());
        if (n == static_cast<std::size_t>(poker::kShortDeckHandClasses)) {
            std::memcpy(out.data(), src, n * sizeof(double));
            return true;
        }
        if (n == 169) {
            for (int i = 0; i < 169; ++i) {
                const int cls = poker::short_deck_hand81_from_hand169(i);
                if (cls >= 0) {
                    out[static_cast<std::size_t>(cls)] += src[i];
                } else if (src[i] != 0.0) {
                    if (err) {
                        *err = "169-class range has weight on a 2-5 hand; short deck uses ranks 6-A (81 classes)";
                    }
                    return false;
                }
            }
            return true;
        }
        if (err) {
            *err = "range Float64Array must have length 81 (6+ classes) or 169 (2-5 weights must be 0)";
        }
        return false;
    }
    if (v.IsArray()) {
        const Napi::Array arr = v.As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            if (!arr.Get(i).IsString()) {
                if (err) {
                    *err = "range string[] entries must be notations like \"AKs\"";
                }
                return false;
            }
            try {
                const int cls = poker::short_deck_hand81_from_notation(arr.Get(i).As<Napi::String>().Utf8Value());
                out[static_cast<std::size_t>(cls)] = 1.0;
            } catch (const std::exception& e) {
                if (err) {
                    *err = e.what();
                }
                return false;
            }
        }
        return true;
    }
    if (!v.IsObject()) {
        if (err) {
            *err = "range must be string[] notations, Float64Array(81|169), or { indices, weights }";
        }
        return false;
    }
    const Napi::Object o = v.As<Napi::Object>();
    const Napi::Array names = o.GetPropertyNames();
    bool any = false;
    for (uint32_t i = 0; i < names.Length(); ++i) {
        const std::string key = names.Get(i).As<Napi::String>().Utf8Value();
        const Napi::Value val = o.Get(key);
        if (!val.IsNumber()) {
            continue;
        }
        try {
            const int cls = poker::short_deck_hand81_from_notation(key);
            out[static_cast<std::size_t>(cls)] = val.As<Napi::Number>().DoubleValue();
            any = true;
        } catch (const std::exception& e) {
            if (err) {
                *err = e.what();
            }
            return false;
        }
    }
    if (!any) {
        if (err) {
            *err = "range object needs notation keys (AKs, QQ, …) or a typed array";
        }
        return false;
    }
    return true;
}

double opt_num(const Napi::Object& o, const char* key, double def) {
    if (o.Has(key) && o.Get(key).IsNumber()) {
        return o.Get(key).As<Napi::Number>().DoubleValue();
    }
    return def;
}

int opt_int(const Napi::Object& o, const char* key, int def) {
    if (o.Has(key) && o.Get(key).IsNumber()) {
        return o.Get(key).As<Napi::Number>().Int32Value();
    }
    return def;
}

}  // namespace

Napi::Value EvaluateShortDeckBestHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "evaluateShortDeckBestHand(cards: CardInput)");
    std::vector<poker::Card> cards;
    if (!try_parse_cards_from_js(env, info[0], cards)) {
        return env.Null();
    }
    POKER_TRY(env, { return short_eval_to_object(env, poker::evaluate_short_deck_best_hand(cards)); });
}

Napi::Value EvaluateShortDeckHandStrength(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_hole_and_board(env, info, hole, board,
                                  "evaluateShortDeckHandStrength(holeCards: CardInput, board: CardInput)")) {
        return env.Null();
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, static_cast<double>(poker::evaluate_short_deck_hand_strength(hole, board)));
    });
}

Napi::Value EvaluateShortDeckCategory(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    if (!try_parse_hole_and_board(env, info, hole, board,
                                  "evaluateShortDeckCategory(holeCards: CardInput, board: CardInput)")) {
        return env.Null();
    }
    POKER_TRY(env, {
        const poker::ShortDeckRank r = poker::evaluate_short_deck_category(hole, board);
        return hand_rank_string_interned(env, poker::short_deck_rank_to_label(r));
    });
}

Napi::Value ExactHuShortDeckEquityVsKnown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactHuShortDeckEquityVsKnown(heroHoleCards, villainHoleCards, boardCards)");
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
        return Napi::Number::New(env, poker::exact_hu_short_deck_equity_vs_known(hero, villain, board));
    });
}

Napi::Value SimulateShortDeckEquityVsRandom(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4 && info[2].IsNumber() && info[3].IsNumber(),
                  "simulateShortDeckEquityVsRandom(heroHoleCards, boardCards, numSimulations, seed)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const int sims = info[2].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = info[3].As<Napi::Number>().Uint32Value();
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::simulate_short_deck_equity_vs_random(hero, board, sims, rng));
    });
}

Napi::Value SimulateShortDeckEquityVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5 && info[3].IsNumber() && info[4].IsNumber(),
                  "simulateShortDeckEquityVsRange(heroHoleCards, boardCards, range, numSimulations, seed)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::array<double, poker::kShortDeckHandClasses> weights{};
    if (!parse_class_weights(env, info[2], weights, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::uint64_t dead = 0;
    for (const poker::Card& c : hero) {
        dead |= (1ULL << poker::deck_index_from_card(c));
    }
    for (const poker::Card& c : board) {
        dead |= (1ULL << poker::deck_index_from_card(c));
    }
    const poker::SparseRange range = poker::short_deck_range_from_class_weights(weights, dead);
    const int sims = info[3].As<Napi::Number>().Int32Value();
    const std::uint32_t seed = info[4].As<Napi::Number>().Uint32Value();
    std::mt19937 rng(seed);
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::simulate_short_deck_equity_vs_range(hero, board, range, sims, rng));
    });
}

Napi::Value ShortDeckStraightIsWheel(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "shortDeckStraightIsWheel(cards: CardInput)");
    std::vector<poker::Card> cards;
    if (!try_parse_cards_from_js(env, info[0], cards)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::short_deck_straight_is_wheel(cards)); });
}

Napi::Value ShortDeckRemainingComboCount(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "shortDeckRemainingComboCount(deadCards: CardInput)");
    std::vector<poker::Card> dead;
    if (!try_parse_cards_from_js(env, info[0], dead)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Number::New(env, poker::short_deck_remaining_combo_count(dead)); });
}

Napi::Value ShortDeckNashHuJamRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "shortDeckNashHuJamRange(stackBb | options)");
    poker::ShortDeckNashSpec spec;
    if (info[0].IsNumber()) {
        const double stack_bb = info[0].As<Napi::Number>().DoubleValue();
        spec.small_blind = info.Length() >= 2 && info[1].IsNumber() ? info[1].As<Napi::Number>().DoubleValue() : 0.5;
        spec.big_blind = info.Length() >= 3 && info[2].IsNumber() ? info[2].As<Napi::Number>().DoubleValue() : 1.0;
        spec.ante = info.Length() >= 4 && info[3].IsNumber() ? info[3].As<Napi::Number>().DoubleValue() : 0.0;
        spec.hero_stack = stack_bb * spec.big_blind;
        spec.villain_stack = stack_bb * spec.big_blind;
        spec.hero_posted = spec.small_blind;
        spec.villain_posted = spec.big_blind;
    } else if (info[0].IsObject()) {
        const Napi::Object o = info[0].As<Napi::Object>();
        spec.small_blind = opt_num(o, "smallBlind", spec.small_blind);
        spec.big_blind = opt_num(o, "bigBlind", spec.big_blind);
        spec.ante = opt_num(o, "ante", spec.ante);
        spec.max_iterations = opt_int(o, "maxIterations", spec.max_iterations);
        spec.tolerance = opt_num(o, "tolerance", spec.tolerance);
        spec.equity_iterations = opt_int(o, "equityIterations", spec.equity_iterations);
        if (o.Has("equitySeed") && o.Get("equitySeed").IsNumber()) {
            spec.equity_seed = o.Get("equitySeed").As<Napi::Number>().Uint32Value();
        }
        const double stack_bb = opt_num(o, "stackBb", 10.0);
        spec.hero_stack = stack_bb * spec.big_blind;
        spec.villain_stack = stack_bb * spec.big_blind;
        spec.hero_posted = spec.small_blind;
        spec.villain_posted = spec.big_blind;
    } else {
        POKER_FAIL_TYPE(env, "shortDeckNashHuJamRange: expected stackBb or options object");
    }
    POKER_TRY(env, {
        const auto jam = poker::short_deck_nash_hu_jam_range(spec);
        Napi::ArrayBuffer buf = Napi::ArrayBuffer::New(env, poker::kShortDeckHandClasses * sizeof(double));
        std::memcpy(buf.Data(), jam.data(), poker::kShortDeckHandClasses * sizeof(double));
        return Napi::Float64Array::New(env, poker::kShortDeckHandClasses, buf, 0);
    });
}

Napi::Value ShortDeckVsHoldemCategoryFlip(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "shortDeckVsHoldemCategoryFlip(cards: CardInput)");
    std::vector<poker::Card> cards;
    if (!try_parse_cards_from_js(env, info[0], cards)) {
        return env.Null();
    }
    POKER_TRY(env, { return Napi::Boolean::New(env, poker::short_deck_vs_holdem_category_flip(cards)); });
}
