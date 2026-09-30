#pragma once

#include <napi.h>

Napi::Value IcmPayoutsAfterBlindPost(const Napi::CallbackInfo& info);
Napi::Value IcmCallingBubbleFactor(const Napi::CallbackInfo& info);
Napi::Value FgsPayoutsBlindSchedule(const Napi::CallbackInfo& info);
Napi::Value IcmDeadPotDollarEv(const Napi::CallbackInfo& info);
