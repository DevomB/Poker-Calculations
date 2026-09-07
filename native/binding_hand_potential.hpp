#pragma once

#include <napi.h>

Napi::Value HandStrengthVsRange(const Napi::CallbackInfo& info);
Napi::Value PositivePotentialVsRange(const Napi::CallbackInfo& info);
Napi::Value NegativePotentialVsRange(const Napi::CallbackInfo& info);
Napi::Value EffectiveHandStrength(const Napi::CallbackInfo& info);
Napi::Value EffectiveHandStrengthSquared(const Napi::CallbackInfo& info);
Napi::Value HandPotentialBreakdown(const Napi::CallbackInfo& info);
Napi::Value TwoStreetPositivePotential(const Napi::CallbackInfo& info);
Napi::Value TwoStreetNegativePotential(const Napi::CallbackInfo& info);
Napi::Value EquityBucketFromEhs(const Napi::CallbackInfo& info);
Napi::Value ComboEhsTableVsRange(const Napi::CallbackInfo& info);
