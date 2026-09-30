#pragma once

#include <napi.h>

Napi::Value NormalizeSparseRange(const Napi::CallbackInfo& info);
Napi::Value PruneRangeByMinWeight(const Napi::CallbackInfo& info);
Napi::Value MergeSparseRanges(const Napi::CallbackInfo& info);
Napi::Value IntersectSparseRanges(const Napi::CallbackInfo& info);
Napi::Value SubtractSparseRange(const Napi::CallbackInfo& info);
Napi::Value RangeComboCount(const Napi::CallbackInfo& info);
Napi::Value RangeShannonEntropy(const Napi::CallbackInfo& info);
Napi::Value RangeGiniCoefficient(const Napi::CallbackInfo& info);
Napi::Value RangeCoverageFraction(const Napi::CallbackInfo& info);
Napi::Value RangeWeightTopKMass(const Napi::CallbackInfo& info);
Napi::Value RangeDistanceL1(const Napi::CallbackInfo& info);
Napi::Value RangeDistanceL2(const Napi::CallbackInfo& info);
Napi::Value RangeDistanceJensenShannon(const Napi::CallbackInfo& info);
Napi::Value RangeCosineSimilarity(const Napi::CallbackInfo& info);
Napi::Value RangeTopCombos(const Napi::CallbackInfo& info);
Napi::Value RangeBucketWeightsByHandClass(const Napi::CallbackInfo& info);
Napi::Value RangeBucketWeightsByNotation(const Napi::CallbackInfo& info);
Napi::Value RangeFromNotationWeights(const Napi::CallbackInfo& info);
Napi::Value RangeBlockerPressureByCard(const Napi::CallbackInfo& info);
Napi::Value RangeRemovalSensitivityVsHero(const Napi::CallbackInfo& info);
Napi::Value BlockerMatrixByCard(const Napi::CallbackInfo& info);

Napi::Value GeometricStreetSizingPlan(const Napi::CallbackInfo& info);
Napi::Value ThinValueMargin(const Napi::CallbackInfo& info);
Napi::Value BetSizingIndifferencePoint(const Napi::CallbackInfo& info);

Napi::Value OpponentFoldToCbetPosterior(const Napi::CallbackInfo& info);
Napi::Value OpponentAggressionFactor(const Napi::CallbackInfo& info);
Napi::Value OpponentRangeElasticityFromSizing(const Napi::CallbackInfo& info);
Napi::Value VillainPolarizedRangeScore(const Napi::CallbackInfo& info);

Napi::Value LegalActionSummary(const Napi::CallbackInfo& info);
Napi::Value ActionMaskFromState(const Napi::CallbackInfo& info);
Napi::Value NormalizeBotConfig(const Napi::CallbackInfo& info);
Napi::Value ValidatePokerState(const Napi::CallbackInfo& info);
Napi::Value StateToFeatureVector(const Napi::CallbackInfo& info);
Napi::Value DecideActionWithDiagnostics(const Napi::CallbackInfo& info);
Napi::Value RunBotPolicyBatch(const Napi::CallbackInfo& info);
