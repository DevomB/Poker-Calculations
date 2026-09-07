#pragma once

#include <napi.h>

Napi::Value RegretMatchingStrategy(const Napi::CallbackInfo& info);
Napi::Value CfrRiverBetCallFoldSolve(const Napi::CallbackInfo& info);
Napi::Value BestResponseRiver(const Napi::CallbackInfo& info);
Napi::Value ExploitabilityRiver(const Napi::CallbackInfo& info);
Napi::Value CfrHeadsUpPushFoldSolve(const Napi::CallbackInfo& info);
Napi::Value FictitiousPlayRiver(const Napi::CallbackInfo& info);
Napi::Value EvOfStrategyProfile(const Napi::CallbackInfo& info);
Napi::Value StrategySupportSize(const Napi::CallbackInfo& info);
Napi::Value CfrNodeReachUpdate(const Napi::CallbackInfo& info);
Napi::Value SolveHuRiverCheckBetTree(const Napi::CallbackInfo& info);
