#pragma once

#include <napi.h>

Napi::Value SpinGoPayouts(const Napi::CallbackInfo& info);
Napi::Value SpinGoIcmEv(const Napi::CallbackInfo& info);
Napi::Value SpinGoNashJamCall(const Napi::CallbackInfo& info);
Napi::Value PkoFgsPayouts(const Napi::CallbackInfo& info);
Napi::Value LateRegOverlayEv(const Napi::CallbackInfo& info);
Napi::Value WinnerTakeAllSatelliteEv(const Napi::CallbackInfo& info);
Napi::Value SqueezeEv(const Napi::CallbackInfo& info);
Napi::Value FourBetJamEv(const Napi::CallbackInfo& info);
Napi::Value IsoRaiseVsLimpersEv(const Napi::CallbackInfo& info);
Napi::Value ThreeBetPotCommitEv(const Napi::CallbackInfo& info);
