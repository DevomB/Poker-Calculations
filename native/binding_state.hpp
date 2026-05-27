#pragma once

#include <napi.h>

#include "poker/bot_config.hpp"
#include "poker/game_state.hpp"
#include "poker/opponent_model.hpp"
#include "poker/types.hpp"

#include <optional>
#include <string>
#include <vector>

#include "poker/card.hpp"

namespace poker_bind {

[[nodiscard]] bool parse_game_state(const Napi::Object& src, poker::PokerGameState& out, std::string* err);

[[nodiscard]] poker::BotConfig parse_bot_config(const Napi::Object& o);

[[nodiscard]] poker::OpponentModel parse_opponent_model(const Napi::Object& o);

[[nodiscard]] void resolve_hero_hole(const poker::PokerGameState& state, int hero_seat,
                                     std::vector<poker::Card>& hero_hole);

struct DecideActionParsed {
    poker::PokerGameState state{};
    poker::BotConfig cfg{};
    std::optional<poker::OpponentModel> opponent;
    int hero_seat{-1};
    std::vector<poker::Card> hero_hole;
};

[[nodiscard]] bool parse_state_input(const Napi::Value& v, poker::PokerGameState& out, std::string* err);

[[nodiscard]] bool parse_decide_action_inputs(const Napi::CallbackInfo& info, DecideActionParsed& out,
                                              std::string* err);

[[nodiscard]] const char* action_name(poker::Action a);

[[nodiscard]] Napi::Object poker_state_to_js(Napi::Env env, const poker::PokerGameState& state);

Napi::Value EncodePokerState(const Napi::CallbackInfo& info);
Napi::Value DecodePokerState(const Napi::CallbackInfo& info);

}  // namespace poker_bind
