#pragma once

#include <napi.h>

#include "poker/bot_config.hpp"
#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/game_state.hpp"
#include "poker/opponent_model.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace poker_async {

using FloatWorkFn = std::function<double(const poker::CancelPredicate*)>;

[[nodiscard]] Napi::Promise enqueue_float_work(Napi::Env env, FloatWorkFn run, Napi::Value signal = {});

[[nodiscard]] Napi::Promise enqueue_decide_action(Napi::Env env, poker::PokerGameState state,
                                                   std::vector<poker::Card> hero_hole,
                                                   poker::BotConfig cfg,
                                                   std::optional<poker::OpponentModel> opponent,
                                                   int hero_seat, Napi::Value signal = {});

[[nodiscard]] Napi::Promise enqueue_benchmark(Napi::Env env, std::size_t iterations,
                                              Napi::Value signal = {});

}  // namespace poker_async
