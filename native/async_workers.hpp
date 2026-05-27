#pragma once

#include <napi.h>

#include "poker/bot_config.hpp"
#include "poker/card.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/game_state.hpp"
#include "poker/opponent_model.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace poker_async {

[[nodiscard]] Napi::Promise enqueue_float_work(Napi::Env env, std::function<double()> run);

[[nodiscard]] Napi::Promise enqueue_decide_action(Napi::Env env, poker::PokerGameState state,
                                                   std::vector<poker::Card> hero_hole, poker::BotConfig cfg,
                                                   std::optional<poker::OpponentModel> opponent, int hero_seat);

[[nodiscard]] Napi::Promise enqueue_benchmark(Napi::Env env, std::size_t iterations);

}  // namespace poker_async
