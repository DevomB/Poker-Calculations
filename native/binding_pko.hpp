#pragma once

#include <napi.h>

Napi::Value PkoKnockoutProbabilityMatrix(const Napi::CallbackInfo& info);
Napi::Value PkoExpectedBountyCollection(const Napi::CallbackInfo& info);
Napi::Value PkoIcmbuPayouts(const Napi::CallbackInfo& info);
Napi::Value PkoBountyRiskPremium(const Napi::CallbackInfo& info);
Napi::Value PkoCallEvVsShove(const Napi::CallbackInfo& info);
Napi::Value PkoJamEvVsFold(const Napi::CallbackInfo& info);
Napi::Value MysteryBountyExpectedValue(const Napi::CallbackInfo& info);
Napi::Value ProgressiveKoPostedBounty(const Napi::CallbackInfo& info);
Napi::Value PkoCoveringHuntEv(const Napi::CallbackInfo& info);
Napi::Value PkoWinnerTakeRemainingBounties(const Napi::CallbackInfo& info);
