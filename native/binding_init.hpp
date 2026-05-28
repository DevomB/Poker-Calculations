#pragma once

#include <napi.h>

#include "poker/types.hpp"

namespace poker_bind {

void init_binding(Napi::Env env);

[[nodiscard]] Napi::String hand_rank_string_interned(Napi::Env env, poker::HandRank r);

}  // namespace poker_bind
