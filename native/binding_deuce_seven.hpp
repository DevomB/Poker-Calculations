#pragma once

#include <napi.h>

Napi::Value EvaluateDeuceSevenHand(const Napi::CallbackInfo& info);
Napi::Value EvaluateDeuceSevenCategory(const Napi::CallbackInfo& info);
Napi::Value DeuceSevenIsPat(const Napi::CallbackInfo& info);
Napi::Value DeuceSevenDrawEquityVsKnown(const Napi::CallbackInfo& info);
Napi::Value DeuceSevenNutsPat(const Napi::CallbackInfo& info);
Napi::Value DeuceSevenRoughVsSmooth(const Napi::CallbackInfo& info);
Napi::Value DeuceSevenMultiwayShowdown(const Napi::CallbackInfo& info);
