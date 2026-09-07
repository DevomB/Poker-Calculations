#pragma once

#include <napi.h>

Napi::Value EvaluateShortDeckBestHand(const Napi::CallbackInfo& info);
Napi::Value EvaluateShortDeckHandStrength(const Napi::CallbackInfo& info);
Napi::Value EvaluateShortDeckCategory(const Napi::CallbackInfo& info);
Napi::Value ExactHuShortDeckEquityVsKnown(const Napi::CallbackInfo& info);
Napi::Value SimulateShortDeckEquityVsRandom(const Napi::CallbackInfo& info);
Napi::Value SimulateShortDeckEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value ShortDeckStraightIsWheel(const Napi::CallbackInfo& info);
Napi::Value ShortDeckRemainingComboCount(const Napi::CallbackInfo& info);
Napi::Value ShortDeckNashHuJamRange(const Napi::CallbackInfo& info);
Napi::Value ShortDeckVsHoldemCategoryFlip(const Napi::CallbackInfo& info);
