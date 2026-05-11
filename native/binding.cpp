#include <napi.h>

#include "poker/game_state.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/opponent_model.hpp"
#include "poker/poker_math.hpp"
#include "poker/strategy.hpp"
#include "poker/types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* kHandRankNames[] = {"highCard",      "onePair",       "twoPair",    "threeOfAKind", "straight",      "flush",         "fullHouse",  "fourOfAKind", "straightFlush", "royalFlush"};

[[nodiscard]] std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

[[nodiscard]] bool parse_card_string(const std::string& raw, poker::Card& out) {
    const std::string s = trim_copy(raw);
    if (s.empty()) {
        return false;
    }
    int rank = -1;
    std::size_t i = 0;
    if (s.size() >= 3 && s[0] == '1' && s[1] == '0') {
        rank = 8;  // Ten
        i = 2;
    } else if (s.size() >= 2) {
        const char r = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        static constexpr const char* kRanks = "23456789TJQKA";
        for (int k = 0; k < 13; ++k) {
            if (kRanks[k] == r) {
                rank = k;
                break;
            }
        }
        i = 1;
    }
    if (rank < 0 || i >= s.size()) {
        return false;
    }
    const char su = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    static constexpr const char* kSuits = "cdhs";
    int suit = -1;
    for (int k = 0; k < 4; ++k) {
        if (kSuits[k] == su) {
            suit = k;
            break;
        }
    }
    if (suit < 0) {
        return false;
    }
    try {
        out = poker::Card(static_cast<std::uint8_t>(rank), static_cast<std::uint8_t>(suit));
    } catch (...) {
        return false;
    }
    return true;
}

[[nodiscard]] std::vector<poker::Card> parse_card_strings(const Napi::Env& env, const Napi::Array& arr, std::string* err) {
    std::vector<poker::Card> out;
    const uint32_t n = arr.Length();
    out.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Napi::Value v = arr[i];
        if (!v.IsString()) {
            if (err) {
                *err = "cards must be strings like \"Ah\"";
            }
            return {};
        }
        poker::Card c;
        if (!parse_card_string(v.As<Napi::String>().Utf8Value(), c)) {
            if (err) {
                *err = "invalid card at index " + std::to_string(i);
            }
            return {};
        }
        out.push_back(c);
    }
    return out;
}

[[nodiscard]] std::string hand_rank_js(poker::HandRank r) {
    const int idx = static_cast<int>(r);
    if (idx >= 0 && idx < 10) {
        return kHandRankNames[idx];
    }
    return "unknown";
}

[[nodiscard]] Napi::Object eval_to_object(Napi::Env env, const poker::HandEvaluation& e) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("rank", hand_rank_js(poker::hand_category(e)));
    Napi::Array kickers = Napi::Array::New(env, 5);
    for (size_t i = 0; i < e.kickers.size(); ++i) {
        kickers[i] = Napi::Number::New(env, e.kickers[i]);
    }
    o.Set("kickers", kickers);
    return o;
}

[[nodiscard]] std::optional<poker::GamePhase> parse_phase_string(const std::string& s) {
    static const std::unordered_map<std::string, poker::GamePhase> m = {
        {"PreFlop", poker::GamePhase::PreFlop},       {"preflop", poker::GamePhase::PreFlop},
        {"Flop", poker::GamePhase::Flop},             {"flop", poker::GamePhase::Flop},
        {"Turn", poker::GamePhase::Turn},             {"turn", poker::GamePhase::Turn},
        {"River", poker::GamePhase::River},           {"river", poker::GamePhase::River},
        {"Showdown", poker::GamePhase::Showdown},     {"showdown", poker::GamePhase::Showdown},
        {"HandComplete", poker::GamePhase::HandComplete},
        {"handcomplete", poker::GamePhase::HandComplete},
    };
    const auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    }
    return std::nullopt;
}

[[nodiscard]] double get_number_prop(const Napi::Object& o, const char* key, double default_val) {
    if (!o.Has(key)) {
        return default_val;
    }
    const Napi::Value v = o.Get(key);
    if (v.IsNumber()) {
        return v.As<Napi::Number>().DoubleValue();
    }
    return default_val;
}

[[nodiscard]] int get_int_prop(const Napi::Object& o, const char* key, int default_val) {
    return static_cast<int>(std::lround(get_number_prop(o, key, static_cast<double>(default_val))));
}

[[nodiscard]] bool get_bool_prop(const Napi::Object& o, const char* key, bool default_val) {
    if (!o.Has(key)) {
        return default_val;
    }
    const Napi::Value v = o.Get(key);
    if (v.IsBoolean()) {
        return v.As<Napi::Boolean>().Value();
    }
    return default_val;
}

[[nodiscard]] bool parse_game_state(const Napi::Object& src, poker::PokerGameState& out, std::string* err) {
    out = {};
    if (!src.Has("players") || !src.Get("players").IsArray()) {
        if (err) {
            *err = "state.players must be an array";
        }
        return false;
    }
    const Napi::Array players = src.Get("players").As<Napi::Array>();
    const uint32_t pn = players.Length();
    out.players.reserve(pn);
    for (uint32_t i = 0; i < pn; ++i) {
        const Napi::Value pv = players[i];
        if (!pv.IsObject()) {
            if (err) {
                *err = "each player must be an object";
            }
            return false;
        }
        const Napi::Object p = pv.As<Napi::Object>();
        poker::Player pl{};
        if (p.Has("name") && p.Get("name").IsString()) {
            pl.name = p.Get("name").As<Napi::String>().Utf8Value();
        }
        if (!p.Has("holeCards") || !p.Get("holeCards").IsArray()) {
            if (err) {
                *err = "player.holeCards must be an array of card strings";
            }
            return false;
        }
        std::string cerr;
        pl.hole_cards = parse_card_strings(Napi::Env(p.Env()), p.Get("holeCards").As<Napi::Array>(), &cerr);
        if (!cerr.empty()) {
            if (err) {
                *err = cerr;
            }
            return false;
        }
        pl.stack = get_int_prop(p, "stack", 0);
        pl.committed_this_street = get_int_prop(p, "committedThisStreet", 0);
        pl.total_committed_hand = get_int_prop(p, "totalCommittedHand", 0);
        pl.folded = get_bool_prop(p, "folded", false);
        pl.seat = get_int_prop(p, "seat", static_cast<int>(i));
        out.players.push_back(std::move(pl));
    }

    if (!src.Has("communityCards") || !src.Get("communityCards").IsArray()) {
        if (err) {
            *err = "state.communityCards must be an array";
        }
        return false;
    }
    std::string cerr2;
    out.community_cards =
        parse_card_strings(Napi::Env(src.Env()), src.Get("communityCards").As<Napi::Array>(), &cerr2);
    if (!cerr2.empty()) {
        if (err) {
            *err = cerr2;
        }
        return false;
    }

    if (!src.Has("phase") || !src.Get("phase").IsString()) {
        if (err) {
            *err = "state.phase must be a string (e.g. PreFlop)";
        }
        return false;
    }
    const std::string phase_str = src.Get("phase").As<Napi::String>().Utf8Value();
    const auto ph = parse_phase_string(phase_str);
    if (!ph) {
        if (err) {
            *err = "unknown phase: " + phase_str;
        }
        return false;
    }
    out.phase = *ph;

    out.pot = get_int_prop(src, "pot", 0);
    out.current_bet = get_int_prop(src, "currentBet", 0);
    out.button_seat = get_int_prop(src, "buttonSeat", 0);
    out.small_blind = get_int_prop(src, "smallBlind", 1);
    out.big_blind = get_int_prop(src, "bigBlind", 2);
    out.acting_index = get_int_prop(src, "actingIndex", -1);
    out.last_raise_increment = get_int_prop(src, "lastRaiseIncrement", 0);
    out.street_opening_index = get_int_prop(src, "streetOpeningIndex", -1);

    if (!src.Has("actedThisStreet") || !src.Get("actedThisStreet").IsArray()) {
        if (err) {
            *err = "state.actedThisStreet must be an array of booleans";
        }
        return false;
    }
    const Napi::Array acted = src.Get("actedThisStreet").As<Napi::Array>();
    const uint32_t an = acted.Length();
    out.acted_this_street.resize(an);
    for (uint32_t i = 0; i < an; ++i) {
        const Napi::Value v = acted[i];
        out.acted_this_street[i] = v.IsBoolean() && v.As<Napi::Boolean>().Value();
    }

    return true;
}

[[nodiscard]] poker::BotConfig parse_bot_config(const Napi::Object& o) {
    poker::BotConfig cfg{};
    cfg.aggression_threshold = static_cast<float>(get_number_prop(o, "aggressionThreshold", cfg.aggression_threshold));
    cfg.risk_tolerance = static_cast<float>(get_number_prop(o, "riskTolerance", cfg.risk_tolerance));
    cfg.monte_carlo_simulations =
        get_int_prop(o, "monteCarloSimulations", cfg.monte_carlo_simulations);
    cfg.monte_carlo_villains = get_int_prop(o, "monteCarloVillains", cfg.monte_carlo_villains);
    cfg.raise_pot_fraction =
        static_cast<float>(get_number_prop(o, "raisePotFraction", cfg.raise_pot_fraction));
    cfg.opponent_aggression_weight =
        static_cast<float>(get_number_prop(o, "opponentAggressionWeight", cfg.opponent_aggression_weight));
    const double seed_d = get_number_prop(o, "rngSeed", static_cast<double>(cfg.rng_seed));
    cfg.rng_seed = static_cast<std::uint32_t>(std::llround(seed_d));
    return cfg;
}

[[nodiscard]] poker::OpponentModel parse_opponent_model(const Napi::Object& o) {
    poker::OpponentModel m{};
    m.aggression_factor = static_cast<float>(get_number_prop(o, "aggressionFactor", m.aggression_factor));
    m.call_frequency = static_cast<float>(get_number_prop(o, "callFrequency", m.call_frequency));
    m.fold_frequency = static_cast<float>(get_number_prop(o, "foldFrequency", m.fold_frequency));
    return m;
}

[[nodiscard]] const char* action_name(poker::Action a) {
    switch (a) {
        case poker::Action::Fold:
            return "fold";
        case poker::Action::Call:
            return "call";
        case poker::Action::Raise:
            return "raise";
        case poker::Action::Check:
            return "check";
        default:
            return "fold";
    }
}

Napi::Value EvaluateBestHand(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsArray()) {
            throw std::invalid_argument("evaluateBestHand(cards: string[])");
        }
        std::string err;
        auto cards = parse_card_strings(env, info[0].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        if (cards.empty() || cards.size() > 7) {
            throw std::invalid_argument("need 1..7 cards");
        }
        const poker::HandEvaluation e = poker::evaluate_best_hand(cards);
        return eval_to_object(env, e);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EvaluateHandStrength(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsArray() || !info[1].IsArray()) {
            throw std::invalid_argument("evaluateHandStrength(holeCards: string[], board: string[])");
        }
        std::string err;
        auto hole = parse_card_strings(env, info[0].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        auto board = parse_card_strings(env, info[1].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const std::uint64_t s = poker::evaluate_hand_strength(hole, board);
        return Napi::String::New(env, std::to_string(s));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EvaluateHandCategory(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsArray() || !info[1].IsArray()) {
            throw std::invalid_argument("evaluateHandCategory(holeCards: string[], board: string[])");
        }
        std::string err;
        auto hole = parse_card_strings(env, info[0].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        auto board = parse_card_strings(env, info[1].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const poker::HandRank r = poker::evaluate_hand(hole, board);
        return Napi::String::New(env, hand_rank_js(r));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value SimulateHandOutcome(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsArray() || !info[1].IsArray() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "simulateHandOutcome(holeCards, board, numSimulations, seed, villains?)");
        }
        std::string err;
        auto hole = parse_card_strings(env, info[0].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        auto board = parse_card_strings(env, info[1].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const int num_sim = info[2].As<Napi::Number>().Int32Value();
        const std::uint32_t seed = static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
        int villains = 1;
        if (info.Length() >= 5 && info[4].IsNumber()) {
            villains = info[4].As<Napi::Number>().Int32Value();
        }
        std::mt19937 rng(seed);
        const float eq =
            poker::simulate_hand_outcome(hole, board, num_sim, rng, villains);
        return Napi::Number::New(env, static_cast<double>(eq));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ParallelHandSimulation(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 6 || !info[0].IsArray() || !info[1].IsArray()) {
            throw std::invalid_argument(
                "parallelHandSimulation(hole, board, numSimulations, baseSeed, villains, numThreads)");
        }
        std::string err;
        auto hole = parse_card_strings(env, info[0].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        auto board = parse_card_strings(env, info[1].As<Napi::Array>(), &err);
        if (!err.empty()) {
            throw std::invalid_argument(err);
        }
        const int num_sim = info[2].As<Napi::Number>().Int32Value();
        const std::uint32_t base_seed =
            static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
        const int villains = info[4].As<Napi::Number>().Int32Value();
        const std::size_t num_threads = static_cast<std::size_t>(info[5].As<Napi::Number>().Uint32Value());
        const float eq = poker::parallel_hand_simulation(hole, board, num_sim, base_seed, villains, num_threads);
        return Napi::Number::New(env, static_cast<double>(eq));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value DecideAction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsObject()) {
            throw std::invalid_argument("decideAction(state, config, opponentModel?, heroSeat?)");
        }
        poker::PokerGameState state{};
        std::string err;
        if (!parse_game_state(info[0].As<Napi::Object>(), state, &err)) {
            throw std::invalid_argument(err.empty() ? "invalid state" : err);
        }
        const poker::BotConfig cfg = parse_bot_config(info[1].As<Napi::Object>());

        const poker::OpponentModel* opp_ptr = nullptr;
        poker::OpponentModel opp_storage{};
        if (info.Length() >= 3 && info[2].IsObject()) {
            opp_storage = parse_opponent_model(info[2].As<Napi::Object>());
            opp_ptr = &opp_storage;
        }

        int hero_seat = -1;
        if (info.Length() >= 4 && info[3].IsNumber()) {
            hero_seat = info[3].As<Napi::Number>().Int32Value();
        }

        std::vector<poker::Card> hero_hole;
        int resolved = hero_seat;
        if (resolved < 0 && state.acting_index >= 0 &&
            state.acting_index < static_cast<int>(state.players.size())) {
            resolved = state.players[static_cast<std::size_t>(state.acting_index)].seat;
        }
        if (resolved < 0 && !state.players.empty()) {
            resolved = state.players[0].seat;
        }
        for (const auto& p : state.players) {
            if (p.seat == resolved) {
                hero_hole = p.hole_cards;
                break;
            }
        }
        if (hero_hole.empty() && !state.players.empty()) {
            hero_hole = state.players[0].hole_cards;
        }

        const poker::Decision d =
            poker::decide_action(state, hero_hole, cfg, opp_ptr, hero_seat);

        Napi::Object out = Napi::Object::New(env);
        out.Set("action", Napi::String::New(env, action_name(d.action)));
        out.Set("raiseBy", Napi::Number::New(env, d.raise_by));
        return out;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PotOddsRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("potOddsRatio(pot, toCall)");
        }
        const int pot = info[0].As<Napi::Number>().Int32Value();
        const int to_call = info[1].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::pot_odds_ratio(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ExpectedValueCall(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("expectedValueCall(equity, pot, toCall)");
        }
        const double equity = info[0].As<Napi::Number>().DoubleValue();
        const int pot = info[1].As<Napi::Number>().Int32Value();
        const int to_call = info[2].As<Napi::Number>().Int32Value();
        return Napi::Number::New(env, poker::expected_value_call(equity, pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value Spr(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("spr(potChips, effectiveStackChips)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double eff = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::spr(pot, eff));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value EffectiveStack(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() == 0) {
            return Napi::Number::New(env, poker::effective_stack({}));
        }
        std::vector<double> stacks;
        stacks.reserve(info.Length());
        for (std::size_t i = 0; i < info.Length(); ++i) {
            if (!info[i].IsNumber()) {
                throw std::invalid_argument("effectiveStack(...stackChips): all args must be numbers");
            }
            stacks.push_back(info[i].As<Napi::Number>().DoubleValue());
        }
        return Napi::Number::New(env, poker::effective_stack(stacks));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenCallEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("breakevenCallEquity(potBeforeCall, toCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_call_equity(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value MinimumDefenseFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("minimumDefenseFrequency(potBeforeOpponentBet, opponentBetSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::minimum_defense_frequency(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value StackInBigBlinds(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("stackInBigBlinds(stackChips, bigBlind)");
        }
        const double stack = info[0].As<Napi::Number>().DoubleValue();
        const double bb = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::stack_in_big_blinds(stack, bb));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value PotOddsRatioDisplay(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("potOddsRatioDisplay(potBeforeCall, toCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::pot_odds_ratio_display(pot, to_call));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value FormatPotOdds(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("formatPotOdds(potBeforeCall, toCall, decimals?)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        int decimals = 2;
        if (info.Length() >= 3 && info[2].IsNumber()) {
            decimals = info[2].As<Napi::Number>().Int32Value();
        }
        return Napi::String::New(env, poker::format_pot_odds(pot, to_call, decimals));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RuleOfFourEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("ruleOfFourEquity(outs)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::rule_of_four_equity(outs));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value RuleOfTwoEquity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw std::invalid_argument("ruleOfTwoEquity(outs)");
        }
        const double outs = info[0].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::rule_of_two_equity(outs));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ImpliedBreakevenFutureWin(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("impliedBreakevenFutureWin(potBeforeCall, toCall, equity)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        const double equity = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::implied_breakeven_future_win(pot, to_call, equity));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BluffToValueRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("bluffToValueRatio(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::bluff_to_value_ratio(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ValueToBluffRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("valueToBluffRatio(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::value_to_bluff_ratio(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BetAsPotFraction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("betAsPotFraction(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::bet_as_pot_fraction(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value SprAfterCall(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber()) {
            throw std::invalid_argument("sprAfterCall(potBeforeCall, toCall, effectiveStackBeforeCall)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double to_call = info[1].As<Napi::Number>().DoubleValue();
        const double stack = info[2].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::spr_after_call(pot, to_call, stack));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value CommitmentRatio(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("commitmentRatio(toCall, effectiveStackBeforeCall)");
        }
        const double to_call = info[0].As<Napi::Number>().DoubleValue();
        const double stack = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::commitment_ratio(to_call, stack));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value AlphaFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("alphaFrequency(potBeforeBet, betSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::alpha_frequency(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquityPureBluff(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            throw std::invalid_argument("breakevenFoldEquityPureBluff(potBeforeHeroBet, heroBetOrCallSize)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double bet = info[1].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env, poker::breakeven_fold_equity_pure_bluff(pot, bet));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value BreakevenFoldEquitySemiBluff(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    try {
        if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() ||
            !info[3].IsNumber()) {
            throw std::invalid_argument(
                "breakevenFoldEquitySemiBluff(potBeforeHeroBet, heroBetSize, equityWhenCalled, "
                "totalPotIfCalled)");
        }
        const double pot = info[0].As<Napi::Number>().DoubleValue();
        const double hero_bet = info[1].As<Napi::Number>().DoubleValue();
        const double eq = info[2].As<Napi::Number>().DoubleValue();
        const double total = info[3].As<Napi::Number>().DoubleValue();
        return Napi::Number::New(env,
                                 poker::breakeven_fold_equity_semi_bluff(pot, hero_bet, eq, total));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Object RegisterExports(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "evaluateBestHand"), Napi::Function::New(env, EvaluateBestHand));
    exports.Set(Napi::String::New(env, "evaluateHandStrength"),
                Napi::Function::New(env, EvaluateHandStrength));
    exports.Set(Napi::String::New(env, "evaluateHandCategory"),
                Napi::Function::New(env, EvaluateHandCategory));
    exports.Set(Napi::String::New(env, "simulateHandOutcome"),
                Napi::Function::New(env, SimulateHandOutcome));
    exports.Set(Napi::String::New(env, "parallelHandSimulation"),
                Napi::Function::New(env, ParallelHandSimulation));
    exports.Set(Napi::String::New(env, "decideAction"), Napi::Function::New(env, DecideAction));
    exports.Set(Napi::String::New(env, "potOddsRatio"), Napi::Function::New(env, PotOddsRatio));
    exports.Set(Napi::String::New(env, "expectedValueCall"), Napi::Function::New(env, ExpectedValueCall));
    exports.Set(Napi::String::New(env, "spr"), Napi::Function::New(env, Spr));
    exports.Set(Napi::String::New(env, "effectiveStack"), Napi::Function::New(env, EffectiveStack));
    exports.Set(Napi::String::New(env, "breakevenCallEquity"),
                Napi::Function::New(env, BreakevenCallEquity));
    exports.Set(Napi::String::New(env, "minimumDefenseFrequency"),
                Napi::Function::New(env, MinimumDefenseFrequency));
    exports.Set(Napi::String::New(env, "stackInBigBlinds"), Napi::Function::New(env, StackInBigBlinds));
    exports.Set(Napi::String::New(env, "potOddsRatioDisplay"),
                Napi::Function::New(env, PotOddsRatioDisplay));
    exports.Set(Napi::String::New(env, "formatPotOdds"), Napi::Function::New(env, FormatPotOdds));
    exports.Set(Napi::String::New(env, "ruleOfFourEquity"), Napi::Function::New(env, RuleOfFourEquity));
    exports.Set(Napi::String::New(env, "ruleOfTwoEquity"), Napi::Function::New(env, RuleOfTwoEquity));
    exports.Set(Napi::String::New(env, "impliedBreakevenFutureWin"),
                Napi::Function::New(env, ImpliedBreakevenFutureWin));
    exports.Set(Napi::String::New(env, "bluffToValueRatio"), Napi::Function::New(env, BluffToValueRatio));
    exports.Set(Napi::String::New(env, "valueToBluffRatio"), Napi::Function::New(env, ValueToBluffRatio));
    exports.Set(Napi::String::New(env, "betAsPotFraction"), Napi::Function::New(env, BetAsPotFraction));
    exports.Set(Napi::String::New(env, "sprAfterCall"), Napi::Function::New(env, SprAfterCall));
    exports.Set(Napi::String::New(env, "commitmentRatio"), Napi::Function::New(env, CommitmentRatio));
    exports.Set(Napi::String::New(env, "alphaFrequency"), Napi::Function::New(env, AlphaFrequency));
    exports.Set(Napi::String::New(env, "breakevenFoldEquityPureBluff"),
                Napi::Function::New(env, BreakevenFoldEquityPureBluff));
    exports.Set(Napi::String::New(env, "breakevenFoldEquitySemiBluff"),
                Napi::Function::New(env, BreakevenFoldEquitySemiBluff));
    return exports;
}

}  // namespace

Napi::Object Init(Napi::Env env, Napi::Object exports) { return RegisterExports(env, exports); }

NODE_API_MODULE(poker_calculations, Init)
