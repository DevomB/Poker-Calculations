#pragma once

#include <napi.h>

Napi::Value IcmShapleyValues(const Napi::CallbackInfo& info);
Napi::Value IcmHarvilleStackJacobian(const Napi::CallbackInfo& info);
Napi::Value IcmHarvilleSkillAdjustedPayouts(const Napi::CallbackInfo& info);
Napi::Value IcmFieldPressureIndex(const Napi::CallbackInfo& info);
Napi::Value IcmChopNegotiationAnalysis(const Napi::CallbackInfo& info);
Napi::Value TournamentDuelAbsorptionProbabilities(const Napi::CallbackInfo& info);
Napi::Value SidePotLayerTournamentEvDelta(const Napi::CallbackInfo& info);
