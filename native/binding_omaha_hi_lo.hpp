#pragma once

#include <napi.h>

Napi::Value EvaluateOmahaLoHand(const Napi::CallbackInfo& info);
Napi::Value OmahaLoQualifies(const Napi::CallbackInfo& info);
Napi::Value EvaluateOmahaHiLo(const Napi::CallbackInfo& info);
Napi::Value ExactHuOmahaHiLoEquity(const Napi::CallbackInfo& info);
Napi::Value SimulateOmahaHiLoEquity(const Napi::CallbackInfo& info);
Napi::Value OmahaScoopProbabilityMc(const Napi::CallbackInfo& info);
Napi::Value OmahaQuarterProbabilityMc(const Napi::CallbackInfo& info);
Napi::Value OmahaLoNutsOnBoard(const Napi::CallbackInfo& info);
Napi::Value OmahaHiLoNuttedness(const Napi::CallbackInfo& info);
Napi::Value OmahaHiLoMultiwayMc(const Napi::CallbackInfo& info);
