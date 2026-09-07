#pragma once

#include <napi.h>

Napi::Value NashHeadsUpJamRange(const Napi::CallbackInfo& info);
Napi::Value NashHeadsUpCallRange(const Napi::CallbackInfo& info);
Napi::Value NashHeadsUpJamCallSolve(const Napi::CallbackInfo& info);
Napi::Value NashBlindVsBlindSolve(const Napi::CallbackInfo& info);
Napi::Value NashFirstInJamRange(const Napi::CallbackInfo& info);
Napi::Value NashJamFoldChart169(const Napi::CallbackInfo& info);
Napi::Value NashCallChart169(const Napi::CallbackInfo& info);
Napi::Value NashIndifferenceStackBb(const Napi::CallbackInfo& info);
Napi::Value NashIcmHeadsUpJamCallSolve(const Napi::CallbackInfo& info);
Napi::Value NashMultiwayShoveCall(const Napi::CallbackInfo& info);
