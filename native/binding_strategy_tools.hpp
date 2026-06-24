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

Napi::Value ClassifyBoardTexture(const Napi::CallbackInfo& info);
Napi::Value BoardTextureScore(const Napi::CallbackInfo& info);
Napi::Value BoardWetnessScore(const Napi::CallbackInfo& info);
Napi::Value BoardPairednessIndex(const Napi::CallbackInfo& info);
Napi::Value BoardFlushPressure(const Napi::CallbackInfo& info);
Napi::Value BoardStraightPressure(const Napi::CallbackInfo& info);
Napi::Value BoardNutAdvantageApprox(const Napi::CallbackInfo& info);
Napi::Value BoardRangeInteractionScore(const Napi::CallbackInfo& info);
Napi::Value BoardStaticnessIndex(const Napi::CallbackInfo& info);
Napi::Value BoardTurnVolatility(const Napi::CallbackInfo& info);
Napi::Value BoardRiverScareCardScore(const Napi::CallbackInfo& info);
Napi::Value EnumerateScareCards(const Napi::CallbackInfo& info);
Napi::Value BoardEquityShiftDistribution(const Napi::CallbackInfo& info);
Napi::Value RangeBoardCoverage(const Napi::CallbackInfo& info);
Napi::Value HeroBoardConnectivityScore(const Napi::CallbackInfo& info);
Napi::Value BlockerMatrixByCard(const Napi::CallbackInfo& info);

Napi::Value ExactEquityDistributionVsRange(const Napi::CallbackInfo& info);
Napi::Value ExactEquityPercentileVsRange(const Napi::CallbackInfo& info);
Napi::Value ExactEquityRealizationEstimate(const Napi::CallbackInfo& info);
Napi::Value EquityRealizationPenalty(const Napi::CallbackInfo& info);
Napi::Value RiverCallThresholdDistribution(const Napi::CallbackInfo& info);
Napi::Value TurnBarrelRunoutEvDistribution(const Napi::CallbackInfo& info);
Napi::Value DelayedCbetRunoutScore(const Napi::CallbackInfo& info);
Napi::Value ProtectionBetBenefit(const Napi::CallbackInfo& info);
Napi::Value EquityDenialValue(const Napi::CallbackInfo& info);
Napi::Value ShowdownValueIndex(const Napi::CallbackInfo& info);

Napi::Value CbetSizeEvGrid(const Napi::CallbackInfo& info);
Napi::Value ProbeBetEvGrid(const Napi::CallbackInfo& info);
Napi::Value CheckRaiseSemiBluffEv(const Napi::CallbackInfo& info);
Napi::Value OverbetPolarizationScore(const Napi::CallbackInfo& info);
Napi::Value GeometricStreetSizingPlan(const Napi::CallbackInfo& info);
Napi::Value RiverValueBetThreshold(const Napi::CallbackInfo& info);
Napi::Value RiverBluffCandidateScore(const Napi::CallbackInfo& info);
Napi::Value ThinValueMargin(const Napi::CallbackInfo& info);
Napi::Value BetSizingIndifferencePoint(const Napi::CallbackInfo& info);
Napi::Value MultiStreetStackOffThreshold(const Napi::CallbackInfo& info);
Napi::Value FoldEquityNeededByStreetPlan(const Napi::CallbackInfo& info);
Napi::Value BluffCatchDecisionScore(const Napi::CallbackInfo& info);
Napi::Value BlockerAwareBluffFrequency(const Napi::CallbackInfo& info);
Napi::Value ValueTargetingScore(const Napi::CallbackInfo& info);

Napi::Value OpponentFoldToCbetPosterior(const Napi::CallbackInfo& info);
Napi::Value OpponentAggressionFactor(const Napi::CallbackInfo& info);
Napi::Value OpponentShowdownBiasEstimate(const Napi::CallbackInfo& info);
Napi::Value OpponentRangeElasticityFromSizing(const Napi::CallbackInfo& info);
Napi::Value ExploitativeBetSizeAdjustment(const Napi::CallbackInfo& info);
Napi::Value ExploitativeCallThresholdAdjustment(const Napi::CallbackInfo& info);
Napi::Value VillainLineRangeShift(const Napi::CallbackInfo& info);
Napi::Value VillainCappedRangeScore(const Napi::CallbackInfo& info);
Napi::Value VillainPolarizedRangeScore(const Napi::CallbackInfo& info);
Napi::Value VillainFloatFrequencyEstimate(const Napi::CallbackInfo& info);

Napi::Value LegalActionSummary(const Napi::CallbackInfo& info);
Napi::Value ActionMaskFromState(const Napi::CallbackInfo& info);
Napi::Value NormalizeBotConfig(const Napi::CallbackInfo& info);
Napi::Value ValidatePokerState(const Napi::CallbackInfo& info);
Napi::Value StateToFeatureVector(const Napi::CallbackInfo& info);
Napi::Value ActionEvBreakdown(const Napi::CallbackInfo& info);
Napi::Value DecideActionWithDiagnostics(const Napi::CallbackInfo& info);
Napi::Value ExplainDecisionFactors(const Napi::CallbackInfo& info);
Napi::Value CandidateActionSet(const Napi::CallbackInfo& info);
Napi::Value RunBotPolicyBatch(const Napi::CallbackInfo& info);
