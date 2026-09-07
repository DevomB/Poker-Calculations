#pragma once

#include <napi.h>

Napi::Value EvaluateOmahaBestHand(const Napi::CallbackInfo& info);
Napi::Value EvaluateOmahaHandStrength(const Napi::CallbackInfo& info);
Napi::Value ExactHuOmahaEquityVsKnown(const Napi::CallbackInfo& info);
Napi::Value SimulateOmahaEquityVsRandom(const Napi::CallbackInfo& info);
Napi::Value SimulateOmahaEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value OmahaComboCount(const Napi::CallbackInfo& info);
Napi::Value OmahaNutsOnBoard(const Napi::CallbackInfo& info);
Napi::Value OmahaWrapDrawOuts(const Napi::CallbackInfo& info);
Napi::Value OmahaNuttednessScore(const Napi::CallbackInfo& info);
Napi::Value OmahaMultiwayEquityMc(const Napi::CallbackInfo& info);
