#include "binding_state.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"

#include "poker/state_codec.hpp"

#include <cmath>
#include <cstring>
#include <utility>

namespace poker_bind {

const char* action_name(poker::Action a) {
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

namespace {

[[nodiscard]] std::optional<poker::GamePhase> parse_phase_string(const std::string& s) {
    static constexpr std::pair<const char*, poker::GamePhase> kPhases[] = {
        {"PreFlop", poker::GamePhase::PreFlop},
        {"preflop", poker::GamePhase::PreFlop},
        {"Flop", poker::GamePhase::Flop},
        {"flop", poker::GamePhase::Flop},
        {"Turn", poker::GamePhase::Turn},
        {"turn", poker::GamePhase::Turn},
        {"River", poker::GamePhase::River},
        {"river", poker::GamePhase::River},
        {"Showdown", poker::GamePhase::Showdown},
        {"showdown", poker::GamePhase::Showdown},
        {"HandComplete", poker::GamePhase::HandComplete},
        {"handcomplete", poker::GamePhase::HandComplete},
    };
    for (const auto& [label, phase] : kPhases) {
        if (s == label) {
            return phase;
        }
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

const char* phase_name(poker::GamePhase ph) {
    switch (ph) {
        case poker::GamePhase::PreFlop:
            return "PreFlop";
        case poker::GamePhase::Flop:
            return "Flop";
        case poker::GamePhase::Turn:
            return "Turn";
        case poker::GamePhase::River:
            return "River";
        case poker::GamePhase::Showdown:
            return "Showdown";
        case poker::GamePhase::HandComplete:
            return "HandComplete";
        default:
            return "PreFlop";
    }
}

}  // namespace

bool parse_game_state(const Napi::Object& src, poker::PokerGameState& out, std::string* err) {
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
        if (!p.Has("holeCards")) {
            if (err) {
                *err = "player.holeCards is required (string[] or Uint8Array)";
            }
            return false;
        }
        std::string cerr;
        pl.hole_cards = parse_cards_from_js(Napi::Env(p.Env()), p.Get("holeCards"), &cerr);
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

    if (!src.Has("communityCards")) {
        if (err) {
            *err = "state.communityCards is required (string[] or Uint8Array)";
        }
        return false;
    }
    std::string cerr2;
    out.community_cards = parse_cards_from_js(Napi::Env(src.Env()), src.Get("communityCards"), &cerr2);
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

poker::BotConfig parse_bot_config(const Napi::Object& o) {
    poker::BotConfig cfg{};
    cfg.aggression_threshold = static_cast<float>(get_number_prop(o, "aggressionThreshold", cfg.aggression_threshold));
    cfg.risk_tolerance = static_cast<float>(get_number_prop(o, "riskTolerance", cfg.risk_tolerance));
    cfg.monte_carlo_simulations = get_int_prop(o, "monteCarloSimulations", cfg.monte_carlo_simulations);
    cfg.monte_carlo_villains = get_int_prop(o, "monteCarloVillains", cfg.monte_carlo_villains);
    cfg.raise_pot_fraction = static_cast<float>(get_number_prop(o, "raisePotFraction", cfg.raise_pot_fraction));
    cfg.opponent_aggression_weight =
        static_cast<float>(get_number_prop(o, "opponentAggressionWeight", cfg.opponent_aggression_weight));
    const double seed_d = get_number_prop(o, "rngSeed", static_cast<double>(cfg.rng_seed));
    cfg.rng_seed = static_cast<std::uint32_t>(std::llround(seed_d));
    return cfg;
}

poker::OpponentModel parse_opponent_model(const Napi::Object& o) {
    poker::OpponentModel m{};
    m.aggression_factor = static_cast<float>(get_number_prop(o, "aggressionFactor", m.aggression_factor));
    m.call_frequency = static_cast<float>(get_number_prop(o, "callFrequency", m.call_frequency));
    m.fold_frequency = static_cast<float>(get_number_prop(o, "foldFrequency", m.fold_frequency));
    return m;
}

void resolve_hero_hole(const poker::PokerGameState& state, int hero_seat, std::vector<poker::Card>& hero_hole) {
    hero_hole.clear();
    int resolved = hero_seat;
    if (resolved < 0 && state.acting_index >= 0 && state.acting_index < static_cast<int>(state.players.size())) {
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
}

bool parse_state_input(const Napi::Value& v, poker::PokerGameState& out, std::string* err) {
    if (v.IsObject()) {
        return parse_game_state(v.As<Napi::Object>(), out, err);
    }
    const std::uint8_t* data = nullptr;
    std::size_t len = 0;
    std::string perr;
    if (packed_card_bytes(v, &data, &len, &perr)) {
        if (!perr.empty()) {
            if (err) {
                *err = perr;
            }
            return false;
        }
        if (!poker::decode_poker_state(data, len, out, err)) {
            return false;
        }
        return true;
    }
    if (err) {
        *err = "state must be NativePokerState object or packed Uint8Array";
    }
    return false;
}

bool parse_decide_action_inputs(const Napi::CallbackInfo& info, DecideActionParsed& out, std::string* err) {
    if (info.Length() < 2) {
        if (err) {
            *err = "decideAction(state, config, opponentModel?, heroSeat?)";
        }
        return false;
    }
    if (!parse_state_input(info[0], out.state, err)) {
        if (err && err->empty()) {
            *err = "invalid state";
        }
        return false;
    }
    if (!info[1].IsObject()) {
        if (err) {
            *err = "config must be an object";
        }
        return false;
    }
    out.cfg = parse_bot_config(info[1].As<Napi::Object>());
    const int opts_idx = poker_bind::trailing_async_options_index(info);
    const int last_idx = opts_idx >= 0 ? opts_idx - 1 : static_cast<int>(info.Length()) - 1;
    out.opponent.reset();
    out.hero_seat = -1;
    if (last_idx >= 3 && info[last_idx].IsNumber()) {
        out.hero_seat = info[last_idx].As<Napi::Number>().Int32Value();
        if (info[2].IsObject() && !poker_bind::is_async_options(info[2])) {
            out.opponent = parse_opponent_model(info[2].As<Napi::Object>());
        }
    } else if (last_idx >= 2 && info[last_idx].IsNumber()) {
        out.hero_seat = info[last_idx].As<Napi::Number>().Int32Value();
    } else if (last_idx >= 2 && info[last_idx].IsObject() && !poker_bind::is_async_options(info[last_idx])) {
        out.opponent = parse_opponent_model(info[last_idx].As<Napi::Object>());
    }
    resolve_hero_hole(out.state, out.hero_seat, out.hero_hole);
    return true;
}

Napi::Object poker_state_to_js(Napi::Env env, const poker::PokerGameState& state) {
    Napi::Object root = Napi::Object::New(env);
    Napi::Array players = Napi::Array::New(env, static_cast<uint32_t>(state.players.size()));
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        const poker::Player& pl = state.players[i];
        Napi::Object p = Napi::Object::New(env);
        p.Set("name", pl.name);
        Napi::Array holes = Napi::Array::New(env, static_cast<uint32_t>(pl.hole_cards.size()));
        for (std::size_t h = 0; h < pl.hole_cards.size(); ++h) {
            holes[static_cast<uint32_t>(h)] = Napi::String::New(env, pl.hole_cards[h].to_string());
        }
        p.Set("holeCards", holes);
        p.Set("stack", pl.stack);
        p.Set("committedThisStreet", pl.committed_this_street);
        p.Set("totalCommittedHand", pl.total_committed_hand);
        p.Set("folded", pl.folded);
        p.Set("seat", pl.seat);
        players[static_cast<uint32_t>(i)] = p;
    }
    root.Set("players", players);
    Napi::Array board = Napi::Array::New(env, static_cast<uint32_t>(state.community_cards.size()));
    for (std::size_t i = 0; i < state.community_cards.size(); ++i) {
        board[static_cast<uint32_t>(i)] = Napi::String::New(env, state.community_cards[i].to_string());
    }
    root.Set("communityCards", board);
    root.Set("phase", phase_name(state.phase));
    root.Set("pot", state.pot);
    root.Set("currentBet", state.current_bet);
    root.Set("buttonSeat", state.button_seat);
    root.Set("smallBlind", state.small_blind);
    root.Set("bigBlind", state.big_blind);
    root.Set("actingIndex", state.acting_index);
    root.Set("lastRaiseIncrement", state.last_raise_increment);
    root.Set("streetOpeningIndex", state.street_opening_index);
    Napi::Array acted = Napi::Array::New(env, static_cast<uint32_t>(state.acted_this_street.size()));
    for (std::size_t i = 0; i < state.acted_this_street.size(); ++i) {
        acted[static_cast<uint32_t>(i)] = Napi::Boolean::New(env, state.acted_this_street[i]);
    }
    root.Set("actedThisStreet", acted);
    return root;
}

Napi::Value EncodePokerState(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            if (info.Length() < 1 || !info[0].IsObject()) {
            POKER_FAIL_TYPE(env, "encodePokerState(state)");
        }
        poker::PokerGameState state{};
        std::string err;
        if (!parse_game_state(info[0].As<Napi::Object>(), state, &err)) {
            POKER_FAIL_TYPE(env, err.empty() ? "invalid state" : err);
        }
        const std::vector<std::uint8_t> bytes = poker::encode_poker_state(state, &err);
        if (bytes.empty()) {
            POKER_FAIL_TYPE(env, err.empty() ? "encode failed" : err);
        }
        Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, bytes.size());
        std::memcpy(ab.Data(), bytes.data(), bytes.size());
        return Napi::Uint8Array::New(env, bytes.size(), ab, 0);

}

Napi::Value DecodePokerState(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            if (info.Length() < 1) {
            POKER_FAIL_TYPE(env, "decodePokerState(bytes)");
        }
        const std::uint8_t* data = nullptr;
        std::size_t len = 0;
        std::string perr;
        if (!packed_card_bytes(info[0], &data, &len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "decodePokerState(bytes: Uint8Array)" : perr);
        }
        poker::PokerGameState state{};
        std::string err;
        if (!poker::decode_poker_state(data, len, state, &err)) {
            POKER_FAIL_TYPE(env, err.empty() ? "decode failed" : err);
        }
        return poker_state_to_js(env, state);

}

}  // namespace poker_bind
