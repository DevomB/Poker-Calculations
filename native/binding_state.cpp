#include "binding_state.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"

#include "poker/state_codec.hpp"
#include "poker/strategy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

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

bool is_absent(const Napi::Object& o, const char* key) {
    if (!o.Has(key)) {
        return true;
    }
    const Napi::Value v = o.Get(key);
    return v.IsUndefined() || v.IsNull();
}

/// Optional integer field. The JS number is checked before conversion, so 2.5 or 1e12 is reported
/// instead of being rounded or wrapped into an int.
void read_int_field(const Napi::Object& o, const char* key, const std::string& label, int& out,
                    std::vector<std::string>& errors) {
    if (is_absent(o, key)) {
        return;
    }
    const Napi::Value v = o.Get(key);
    if (!v.IsNumber()) {
        errors.push_back(label + " must be a number");
        return;
    }
    const double d = v.As<Napi::Number>().DoubleValue();
    if (!std::isfinite(d) || std::trunc(d) != d) {
        errors.push_back(label + " must be a finite integer");
        return;
    }
    if (d < -2147483648.0 || d > 2147483647.0) {
        errors.push_back(label + " must fit in a 32-bit integer");
        return;
    }
    out = static_cast<int>(d);
}

void read_bool_field(const Napi::Object& o, const char* key, const std::string& label, bool& out,
                     std::vector<std::string>& errors) {
    if (is_absent(o, key)) {
        return;
    }
    const Napi::Value v = o.Get(key);
    if (!v.IsBoolean()) {
        errors.push_back(label + " must be a boolean");
        return;
    }
    out = v.As<Napi::Boolean>().Value();
}

/// Config numbers are tuning knobs, not state: non-finite values keep the default and integers
/// saturate instead of wrapping.
[[nodiscard]] int config_int(const Napi::Object& o, const char* key, int default_val) {
    const double d = get_number_prop(o, key, static_cast<double>(default_val));
    if (!std::isfinite(d)) {
        return default_val;
    }
    return static_cast<int>(std::clamp(std::round(d), -2147483648.0, 2147483647.0));
}

[[nodiscard]] float config_float(const Napi::Object& o, const char* key, float default_val) {
    const double d = get_number_prop(o, key, static_cast<double>(default_val));
    return std::isfinite(d) ? static_cast<float>(d) : default_val;
}

/// Seeds reduce modulo 2^32 (negative integers wrap the usual way).
[[nodiscard]] std::uint32_t config_seed(const Napi::Object& o, const char* key, std::uint32_t default_val) {
    const double d = get_number_prop(o, key, static_cast<double>(default_val));
    if (!std::isfinite(d)) {
        return default_val;
    }
    double r = std::fmod(std::round(d), 4294967296.0);
    if (r < 0.0) {
        r += 4294967296.0;
    }
    return static_cast<std::uint32_t>(r);
}

std::string join_errors(const std::vector<std::string>& errors) {
    std::string out;
    for (const std::string& e : errors) {
        if (!out.empty()) {
            out += "; ";
        }
        out += e;
    }
    return out;
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

bool parse_game_state(const Napi::Object& src, poker::PokerGameState& out,
                      std::vector<std::string>& errors) {
    out = {};
    const std::size_t errors_before = errors.size();
    if (!src.Has("players") || !src.Get("players").IsArray()) {
        errors.emplace_back("state.players must be an array");
        return false;
    }
    const Napi::Array players = src.Get("players").As<Napi::Array>();
    const uint32_t pn = players.Length();
    if (pn > 64) {
        errors.emplace_back("players must have 2 to 10 entries (got " + std::to_string(pn) + ")");
        return false;
    }
    out.players.reserve(pn);
    for (uint32_t i = 0; i < pn; ++i) {
        const std::string label = "players[" + std::to_string(i) + "]";
        const Napi::Value pv = players[i];
        if (!pv.IsObject()) {
            errors.push_back(label + " must be an object");
            continue;
        }
        const Napi::Object p = pv.As<Napi::Object>();
        poker::Player pl{};
        pl.seat = static_cast<int>(i);
        if (p.Has("name") && p.Get("name").IsString()) {
            pl.name = p.Get("name").As<Napi::String>().Utf8Value();
        }
        if (!p.Has("holeCards")) {
            errors.push_back(label + ".holeCards is required (string[] or Uint8Array; [] for an unknown hand)");
        } else {
            std::string cerr;
            pl.hole_cards = parse_cards_from_js(Napi::Env(p.Env()), p.Get("holeCards"), &cerr);
            if (!cerr.empty()) {
                errors.push_back(label + ".holeCards: " + cerr);
            }
        }
        read_int_field(p, "stack", label + ".stack", pl.stack, errors);
        read_int_field(p, "committedThisStreet", label + ".committedThisStreet", pl.committed_this_street, errors);
        read_int_field(p, "totalCommittedHand", label + ".totalCommittedHand", pl.total_committed_hand, errors);
        read_bool_field(p, "folded", label + ".folded", pl.folded, errors);
        read_int_field(p, "seat", label + ".seat", pl.seat, errors);
        out.players.push_back(std::move(pl));
    }

    if (!src.Has("communityCards")) {
        errors.emplace_back("state.communityCards is required (string[] or Uint8Array)");
    } else {
        std::string cerr;
        out.community_cards = parse_cards_from_js(Napi::Env(src.Env()), src.Get("communityCards"), &cerr);
        if (!cerr.empty()) {
            errors.push_back("communityCards: " + cerr);
        }
    }

    if (!src.Has("phase") || !src.Get("phase").IsString()) {
        errors.emplace_back("state.phase must be a string (e.g. PreFlop)");
    } else {
        const std::string phase_str = src.Get("phase").As<Napi::String>().Utf8Value();
        const auto ph = parse_phase_string(phase_str);
        if (ph) {
            out.phase = *ph;
        } else {
            errors.push_back("unknown phase: " + phase_str);
        }
    }

    read_int_field(src, "pot", "pot", out.pot, errors);
    read_int_field(src, "currentBet", "currentBet", out.current_bet, errors);
    read_int_field(src, "buttonSeat", "buttonSeat", out.button_seat, errors);
    read_int_field(src, "smallBlind", "smallBlind", out.small_blind, errors);
    read_int_field(src, "bigBlind", "bigBlind", out.big_blind, errors);
    read_int_field(src, "actingIndex", "actingIndex", out.acting_index, errors);
    read_int_field(src, "lastRaiseIncrement", "lastRaiseIncrement", out.last_raise_increment, errors);
    read_int_field(src, "streetOpeningIndex", "streetOpeningIndex", out.street_opening_index, errors);

    if (!src.Has("actedThisStreet") || !src.Get("actedThisStreet").IsArray()) {
        errors.emplace_back("state.actedThisStreet must be an array of booleans, one per player");
    } else {
        const Napi::Array acted = src.Get("actedThisStreet").As<Napi::Array>();
        const uint32_t an = std::min<uint32_t>(acted.Length(), 65);  // >64 is reported as a length mismatch
        out.acted_this_street.resize(an);
        for (uint32_t i = 0; i < an; ++i) {
            const Napi::Value v = acted[i];
            if (!v.IsBoolean()) {
                errors.push_back("actedThisStreet[" + std::to_string(i) + "] must be a boolean");
                continue;
            }
            out.acted_this_street[i] = v.As<Napi::Boolean>().Value();
        }
    }
    return errors.size() == errors_before;
}

bool parse_game_state(const Napi::Object& src, poker::PokerGameState& out, std::string* err) {
    std::vector<std::string> errors;
    if (parse_game_state(src, out, errors)) {
        return true;
    }
    if (err) {
        *err = join_errors(errors);
    }
    return false;
}

poker::BotConfig parse_bot_config(const Napi::Object& o) {
    poker::BotConfig cfg{};
    cfg.aggression_threshold = config_float(o, "aggressionThreshold", cfg.aggression_threshold);
    cfg.risk_tolerance = config_float(o, "riskTolerance", cfg.risk_tolerance);
    cfg.monte_carlo_simulations = config_int(o, "monteCarloSimulations", cfg.monte_carlo_simulations);
    cfg.monte_carlo_villains = config_int(o, "monteCarloVillains", cfg.monte_carlo_villains);
    cfg.raise_pot_fraction = config_float(o, "raisePotFraction", cfg.raise_pot_fraction);
    cfg.opponent_aggression_weight =
        config_float(o, "opponentAggressionWeight", cfg.opponent_aggression_weight);
    cfg.rng_seed = config_seed(o, "rngSeed", cfg.rng_seed);
    return cfg;
}

poker::OpponentModel parse_opponent_model(const Napi::Object& o) {
    poker::OpponentModel m{};
    m.aggression_factor = config_float(o, "aggressionFactor", m.aggression_factor);
    m.call_frequency = config_float(o, "callFrequency", m.call_frequency);
    m.fold_frequency = config_float(o, "foldFrequency", m.fold_frequency);
    return m;
}

void resolve_hero_hole(const poker::PokerGameState& state, int hero_seat, std::vector<poker::Card>& hero_hole) {
    hero_hole.clear();
    const int idx = poker::hero_player_index(state, hero_seat);
    if (idx >= 0) {
        hero_hole = state.players[static_cast<std::size_t>(idx)].hole_cards;
    }
}

bool parse_state_input(const Napi::Value& v, poker::PokerGameState& out, std::vector<std::string>& errors) {
    const std::size_t errors_before = errors.size();
    // A Uint8Array is also an object, so check for PKST bytes first.
    if (v.IsObject() && !v.IsTypedArray() && !v.IsBuffer()) {
        if (!parse_game_state(v.As<Napi::Object>(), out, errors)) {
            return false;
        }
    } else {
        const std::uint8_t* data = nullptr;
        std::size_t len = 0;
        std::string perr;
        if (!packed_card_bytes(v, &data, &len, &perr) || !perr.empty()) {
            errors.push_back(perr.empty() ? "state must be NativePokerState object or packed Uint8Array" : perr);
            return false;
        }
        std::string derr;
        if (!poker::decode_poker_state(data, len, out, &derr)) {
            errors.push_back(derr.empty() ? "invalid PKST bytes" : derr);
            return false;
        }
    }
    // Objects and PKST bytes share one set of semantic checks.
    const std::vector<std::string> semantic = poker::poker_state_errors(out);
    errors.insert(errors.end(), semantic.begin(), semantic.end());
    return errors.size() == errors_before;
}

bool parse_state_input(const Napi::Value& v, poker::PokerGameState& out, std::string* err) {
    std::vector<std::string> errors;
    if (parse_state_input(v, out, errors)) {
        return true;
    }
    if (err) {
        *err = join_errors(errors);
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
    if (last_idx >= 2 && info[last_idx].IsNumber()) {
        const double seat = info[last_idx].As<Napi::Number>().DoubleValue();
        if (!std::isfinite(seat) || std::trunc(seat) != seat) {
            if (err) {
                *err = "heroSeat must be an integer seat number (or -1 for the acting player)";
            }
            return false;
        }
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
        if (!parse_state_input(info[0], state, &err)) {
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
        if (!parse_state_input(info[0], state, &err)) {
            POKER_FAIL_TYPE(env, err.empty() ? "decode failed" : err);
        }
        return poker_state_to_js(env, state);

}

}  // namespace poker_bind
