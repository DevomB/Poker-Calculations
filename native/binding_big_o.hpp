#pragma once

#include <napi.h>

Napi::Value EvaluateBigOBestHand(const Napi::CallbackInfo& info);
Napi::Value EvaluateBigOHandStrength(const Napi::CallbackInfo& info);
Napi::Value ExactHuBigOEquityVsKnown(const Napi::CallbackInfo& info);
Napi::Value SimulateBigOEquityVsRandom(const Napi::CallbackInfo& info);
Napi::Value SimulateBigOEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value BigOComboCount(const Napi::CallbackInfo& info);
Napi::Value BigONutsOnBoard(const Napi::CallbackInfo& info);
Napi::Value BigOMultiwayEquityMc(const Napi::CallbackInfo& info);
