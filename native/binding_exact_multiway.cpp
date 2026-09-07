#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/exact_multiway.hpp"

#include <string>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

bool parse_hole_hands(const Napi::Env& env, const Napi::Value& v,
                      std::vector<std::vector<poker::Card>>& out, std::string* err) {
    if (!v.IsArray()) {
        if (err) {
            *err = "holeHands must be an array of CardInput";
        }
        return false;
    }
    const Napi::Array arr = v.As<Napi::Array>();
    out.clear();
    out.reserve(arr.Length());
    for (uint32_t i = 0; i < arr.Length(); ++i) {
        out.push_back(parse_cards_from_js(env, arr.Get(i), err));
        if (err && !err->empty()) {
            return false;
        }
    }
    return true;
}

bool parse_optional_dead(const Napi::CallbackInfo& info, std::size_t index, std::vector<poker::Card>& dead,
                         std::string* err) {
    if (info.Length() <= index || info[index].IsUndefined() || info[index].IsNull()) {
        return true;
    }
    dead = parse_cards_from_js(info.Env(), info[index], err);
    return err == nullptr || err->empty();
}

Napi::Value doubles_to_js(Napi::Env env, const std::vector<double>& v) {
    return write_f64_vector(env, v, poker_bind::F64ReturnFormat::Array);
}

Napi::Object win_tie_lose_to_js(Napi::Env env, const poker::MultiwayWinTieLose& r) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("win", doubles_to_js(env, r.win));
    o.Set("split", doubles_to_js(env, r.split));
    o.Set("lose", doubles_to_js(env, r.lose));
    return o;
}

}  // namespace

Napi::Value ExactThreeWayEquityKnownHands(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "exactThreeWayEquityKnownHands(hand0, hand1, hand2, boardCards[, deadCards])");
    std::string err;
    const std::vector<poker::Card> a = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> b = parse_cards_from_js(env, info[1], &err);
    const std::vector<poker::Card> c = parse_cards_from_js(env, info[2], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[3], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 4, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return doubles_to_js(env, poker::exact_three_way_equity_known_hands(a, b, c, board, dead)); });
}

Napi::Value ExactThreeWayWinTieLoseKnownHands(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "exactThreeWayWinTieLoseKnownHands(hand0, hand1, hand2, boardCards[, deadCards])");
    std::string err;
    const std::vector<poker::Card> a = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> b = parse_cards_from_js(env, info[1], &err);
    const std::vector<poker::Card> c = parse_cards_from_js(env, info[2], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[3], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 4, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return win_tie_lose_to_js(env, poker::exact_three_way_win_tie_lose_known_hands(a, b, c, board, dead));
    });
}

Napi::Value ExactFourWayEquityKnownHands(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "exactFourWayEquityKnownHands(hand0, hand1, hand2, hand3, boardCards[, deadCards])");
    std::string err;
    const std::vector<poker::Card> a = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> b = parse_cards_from_js(env, info[1], &err);
    const std::vector<poker::Card> c = parse_cards_from_js(env, info[2], &err);
    const std::vector<poker::Card> d = parse_cards_from_js(env, info[3], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[4], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 5, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return doubles_to_js(env, poker::exact_four_way_equity_known_hands(a, b, c, d, board, dead));
    });
}

Napi::Value ExactMultiwayEquityKnownHands(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactMultiwayEquityKnownHands(holeHands, boardCards[, deadCards])");
    std::string err;
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[0], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 2, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return doubles_to_js(env, poker::exact_multiway_equity_known_hands(holes, board, dead)); });
}

Napi::Value ExactMultiwayEquityWithDeadCards(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactMultiwayEquityWithDeadCards(holeHands, boardCards, deadCards)");
    std::string err;
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[0], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    const std::vector<poker::Card> dead = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, { return doubles_to_js(env, poker::exact_multiway_equity_known_hands(holes, board, dead)); });
}

Napi::Value ExactMultiwaySidePotChipEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactMultiwaySidePotChipEv(committedChips, holeHands, boardCards[, deadCards])");
    std::string err;
    std::vector<double> committed;
    if (!read_f64_vector(info[0], "committedChips", committed, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[1], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 3, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const poker::MultiwaySidePotChipEv r =
            poker::exact_multiway_side_pot_chip_ev(committed, holes, board, dead);
        Napi::Object o = Napi::Object::New(env);
        o.Set("chipEv", doubles_to_js(env, r.chip_ev));
        o.Set("layerCount", Napi::Number::New(env, r.layer_count));
        return o;
    });
}

Napi::Value ExactMultiwayAheadFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactMultiwayAheadFrequency(holeHands, boardCards[, deadCards])");
    std::string err;
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[0], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 2, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const poker::MultiwayAheadFrequency r =
            poker::exact_multiway_ahead_frequency(holes, board, dead);
        Napi::Object o = Napi::Object::New(env);
        o.Set("pAheadNow", Napi::Number::New(env, r.p_ahead_now));
        o.Set("pWinShowdown", Napi::Number::New(env, r.p_win_showdown));
        return o;
    });
}

Napi::Value ExactMultiwayTieFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "exactMultiwayTieFrequency(holeHands, boardCards[, deadCards])");
    std::string err;
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[0], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 2, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const poker::MultiwayTieFrequency r = poker::exact_multiway_tie_frequency(holes, board, dead);
        Napi::Object o = Napi::Object::New(env);
        o.Set("pHeroSplit", Napi::Number::New(env, r.p_hero_split));
        o.Set("pAnySplit", Napi::Number::New(env, r.p_any_split));
        return o;
    });
}

Napi::Value ExactMultiwayRunoutCount(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "exactMultiwayRunoutCount(holeHands, boardCards[, deadCards])");
    std::string err;
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[0], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 2, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, static_cast<double>(
                                          poker::exact_multiway_runout_count(holes, board, dead)));
    });
}

Napi::Value ExactMultiwayBestWorstRunout(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactMultiwayBestWorstRunout(holeHands, boardCards[, deadCards])");
    std::string err;
    std::vector<std::vector<poker::Card>> holes;
    if (!parse_hole_hands(env, info[0], holes, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (!parse_optional_dead(info, 2, dead, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const poker::MultiwayBestWorstRunout r =
            poker::exact_multiway_best_worst_runout(holes, board, dead);
        Napi::Object o = Napi::Object::New(env);
        o.Set("supported", Napi::Boolean::New(env, r.supported));
        if (r.supported) {
            o.Set("bestCard", Napi::String::New(env, r.best_card.to_string()));
            o.Set("worstCard", Napi::String::New(env, r.worst_card.to_string()));
            o.Set("bestEquity", Napi::Number::New(env, r.best_equity));
            o.Set("worstEquity", Napi::Number::New(env, r.worst_equity));
        }
        return o;
    });
}
