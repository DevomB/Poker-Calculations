#pragma once

#include <napi.h>

Napi::Value Ehs2BucketsVsRange(const Napi::CallbackInfo& info);
Napi::Value BucketMassFromRange(const Napi::CallbackInfo& info);
Napi::Value FlopBucketStrategyTo1326(const Napi::CallbackInfo& info);
Napi::Value CanonicalFlopCfrKey(const Napi::CallbackInfo& info);
Napi::Value FlopBucketCountDefault(const Napi::CallbackInfo& info);
