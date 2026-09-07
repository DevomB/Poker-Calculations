#pragma once

#include <napi.h>

Napi::Value CanonicalFlopBoard(const Napi::CallbackInfo& info);
Napi::Value CanonicalBoard(const Napi::CallbackInfo& info);
Napi::Value CanonicalHolesAndBoard(const Napi::CallbackInfo& info);
Napi::Value SuitPermFromCanonicalFlop(const Napi::CallbackInfo& info);
Napi::Value ApplySuitPermToCards(const Napi::CallbackInfo& info);
Napi::Value ApplySuitPermToRange1326(const Napi::CallbackInfo& info);
Napi::Value IsomorphicFlopOrbitSize(const Napi::CallbackInfo& info);
Napi::Value CountCanonicalFlops(const Napi::CallbackInfo& info);
Napi::Value IsomorphicFlopIndex(const Napi::CallbackInfo& info);
Napi::Value FlopIndexToCanonical(const Napi::CallbackInfo& info);
