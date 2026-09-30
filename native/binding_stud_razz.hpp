#pragma once

#include <napi.h>

Napi::Value EvaluateStudBestHand(const Napi::CallbackInfo& info);
Napi::Value EvaluateRazzHand(const Napi::CallbackInfo& info);
Napi::Value RazzWheelIsNuts(const Napi::CallbackInfo& info);
Napi::Value ExactHuStudEquityVsKnown(const Napi::CallbackInfo& info);
Napi::Value ExactHuRazzEquityVsKnown(const Napi::CallbackInfo& info);
Napi::Value StudDeadCardDeck(const Napi::CallbackInfo& info);
Napi::Value SimulateStudEquityVsRandom(const Napi::CallbackInfo& info);
Napi::Value SimulateRazzEquityVsRandom(const Napi::CallbackInfo& info);
