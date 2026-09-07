#pragma once

#include <napi.h>

Napi::Value FutureGameSimulationPayouts(const Napi::CallbackInfo& info);
Napi::Value FutureGrowthShare(const Napi::CallbackInfo& info);
Napi::Value IcmPayoutsAfterBlindPost(const Napi::CallbackInfo& info);
Napi::Value IcmJamVsFoldEv(const Napi::CallbackInfo& info);
Napi::Value IcmCallVsFoldEv(const Napi::CallbackInfo& info);
Napi::Value IcmCallingBubbleFactor(const Napi::CallbackInfo& info);
Napi::Value FgsPayoutsBlindSchedule(const Napi::CallbackInfo& info);
Napi::Value IcmStallingEv(const Napi::CallbackInfo& info);
Napi::Value IcmPayJumpSurvivalEv(const Napi::CallbackInfo& info);
Napi::Value IcmDeadPotDollarEv(const Napi::CallbackInfo& info);
