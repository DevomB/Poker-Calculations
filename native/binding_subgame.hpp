#pragma once

#include <napi.h>

Napi::Value MaterializeVillainRangeAfterBlockers(const Napi::CallbackInfo& info);
Napi::Value BayesianRangeUpdateFromAction(const Napi::CallbackInfo& info);
Napi::Value SolveRiverPolarizedIndifferenceBet(const Napi::CallbackInfo& info);
Napi::Value SolveStageMinimaxRegretBet(const Napi::CallbackInfo& info);
Napi::Value ExactInformationRegretVsClairvoyant(const Napi::CallbackInfo& info);
Napi::Value MultiwayEquityIndependenceGap(const Napi::CallbackInfo& info);
Napi::Value SolveSymmetricPushFoldThreshold(const Napi::CallbackInfo& info);
