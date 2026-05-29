#pragma once

#include <napi.h>

Napi::Value ExactHuEquityVsKnownHand(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value SimulateEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeDetailed(const Napi::CallbackInfo& info);
Napi::Value BuildPreflopEquityMatrix(const Napi::CallbackInfo& info);
Napi::Value EquityDeltaIfCardRemoved(const Napi::CallbackInfo& info);
Napi::Value IcmExpectedPayoutsWeitzman(const Napi::CallbackInfo& info);
