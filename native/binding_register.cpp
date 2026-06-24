#include <napi.h>

#include "binding_batch.hpp"
#include "binding_combinatorics.hpp"
#include "binding_cooperative_icm.hpp"
#include "binding_init.hpp"
#include "binding_range.hpp"
#include "binding_state.hpp"
#include "binding_subgame.hpp"
#include "binding_strategy_tools.hpp"

Napi::Value EvaluateBestHand(const Napi::CallbackInfo& info);
Napi::Value EvaluateHandStrength(const Napi::CallbackInfo& info);
Napi::Value EvaluateHandStrengthFast(const Napi::CallbackInfo& info);
Napi::Value BenchmarkEvaluatorThroughput(const Napi::CallbackInfo& info);
Napi::Value BenchmarkEvaluatorThroughputAsync(const Napi::CallbackInfo& info);
Napi::Value EvaluateHandCategory(const Napi::CallbackInfo& info);
Napi::Value ValidateCardString(const Napi::CallbackInfo& info);
Napi::Value CardStringsHaveDuplicate(const Napi::CallbackInfo& info);
Napi::Value CanonicalCardString(const Napi::CallbackInfo& info);
Napi::Value ParseCompactCardList(const Napi::CallbackInfo& info);
Napi::Value CompareBestHands(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcome(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeAsync(const Napi::CallbackInfo& info);
Napi::Value ParallelHandSimulation(const Napi::CallbackInfo& info);
Napi::Value ParallelHandSimulationAsync(const Napi::CallbackInfo& info);
Napi::Value DecideAction(const Napi::CallbackInfo& info);
Napi::Value DecideActionAsync(const Napi::CallbackInfo& info);
Napi::Value PotOddsRatio(const Napi::CallbackInfo& info);
Napi::Value ExpectedValueCall(const Napi::CallbackInfo& info);
Napi::Value ExpectedValueCallWithRake(const Napi::CallbackInfo& info);
Napi::Value Spr(const Napi::CallbackInfo& info);
Napi::Value EffectiveStack(const Napi::CallbackInfo& info);
Napi::Value NormalizedStackFractions(const Napi::CallbackInfo& info);
Napi::Value BreakevenCallEquity(const Napi::CallbackInfo& info);
Napi::Value MinimumDefenseFrequency(const Napi::CallbackInfo& info);
Napi::Value StackInBigBlinds(const Napi::CallbackInfo& info);
Napi::Value PotOddsRatioDisplay(const Napi::CallbackInfo& info);
Napi::Value FormatPotOdds(const Napi::CallbackInfo& info);
Napi::Value BreakevenCallEquityFromPotOddsDisplayRatio(const Napi::CallbackInfo& info);
Napi::Value PotOddsDisplayRatioFromBreakevenCallEquity(const Napi::CallbackInfo& info);
Napi::Value FormatPotOddsReducedFraction(const Napi::CallbackInfo& info);
Napi::Value EquityToWinningOddsAgainst(const Napi::CallbackInfo& info);
Napi::Value WinningOddsAgainstToEquity(const Napi::CallbackInfo& info);
Napi::Value RuleOfFourEquity(const Napi::CallbackInfo& info);
Napi::Value RuleOfTwoEquity(const Napi::CallbackInfo& info);
Napi::Value EstimatedOutsFromRuleOfTwo(const Napi::CallbackInfo& info);
Napi::Value EstimatedOutsFromRuleOfFour(const Napi::CallbackInfo& info);
Napi::Value ImpliedBreakevenFutureWin(const Napi::CallbackInfo& info);
Napi::Value BluffToValueRatio(const Napi::CallbackInfo& info);
Napi::Value ValueToBluffRatio(const Napi::CallbackInfo& info);
Napi::Value BetAsPotFraction(const Napi::CallbackInfo& info);
Napi::Value SprAfterCall(const Napi::CallbackInfo& info);
Napi::Value CommitmentRatio(const Napi::CallbackInfo& info);
Napi::Value AlphaFrequency(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquityPureBluff(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquitySemiBluff(const Napi::CallbackInfo& info);
Napi::Value HypergeometricOneCardHitProbability(const Napi::CallbackInfo& info);
Napi::Value RunnerRunnerBackdoorFlushTwoCardProbability(const Napi::CallbackInfo& info);
Napi::Value FlopToRiverAtLeastOneHitProbability(const Napi::CallbackInfo& info);
Napi::Value FlopToRiverAtLeastOneHitUnionTwoCategories(const Napi::CallbackInfo& info);
Napi::Value FlopToRiverAtLeastOneHitUnionThreeCategories(const Napi::CallbackInfo& info);
Napi::Value FlopToRiverAtLeastOneHitUnionFourCategories(const Napi::CallbackInfo& info);
Napi::Value FlopToRiverAtLeastOneHitDisjointOutsSum(const Napi::CallbackInfo& info);
Napi::Value RunnerRunnerStraightDrawHitProbability(const Napi::CallbackInfo& info);
Napi::Value ReverseImpliedOddsMaxFutureLoss(const Napi::CallbackInfo& info);
Napi::Value GeometricPotAfterMatchedPotFractions(const Napi::CallbackInfo& info);
Napi::Value HarringtonM(const Napi::CallbackInfo& info);
Napi::Value HarringtonMEffective(const Napi::CallbackInfo& info);
Napi::Value HarringtonMEffectiveActiveAntes(const Napi::CallbackInfo& info);
Napi::Value HarringtonQ(const Napi::CallbackInfo& info);
Napi::Value OrbitCostChips(const Napi::CallbackInfo& info);
Napi::Value NlMinimumRaiseToTotal(const Napi::CallbackInfo& info);
Napi::Value PreflopCombosFromNotation(const Napi::CallbackInfo& info);
Napi::Value PreflopCombosFromNotationsList(const Napi::CallbackInfo& info);
Napi::Value HandRankCategoryOrder(const Napi::CallbackInfo& info);
Napi::Value KellyCriterionBinary(const Napi::CallbackInfo& info);
Napi::Value MonteCarloStandardError(const Napi::CallbackInfo& info);
Napi::Value MonteCarloTrialsForStandardErrorBound(const Napi::CallbackInfo& info);
Napi::Value BetaBinomialFoldPosterior(const Napi::CallbackInfo& info);
Napi::Value DuplicationAdjustedOuts(const Napi::CallbackInfo& info);
Napi::Value RiskOfRuinDiffusionApprox(const Napi::CallbackInfo& info);
Napi::Value BankrollForTargetRorDiffusion(const Napi::CallbackInfo& info);
Napi::Value WilsonScoreInterval(const Napi::CallbackInfo& info);
Napi::Value AgrestiCoullInterval(const Napi::CallbackInfo& info);
Napi::Value NormalWaldBinomialInterval(const Napi::CallbackInfo& info);
Napi::Value MonteCarloTrialsForHoeffdingBound(const Napi::CallbackInfo& info);
Napi::Value RakeFromPot(const Napi::CallbackInfo& info);
Napi::Value BreakevenCallEquityWithRake(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquitySemiBluffWithRake(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquityPureBluffWithRake(const Napi::CallbackInfo& info);
Napi::Value MultiwaySymmetricBreakevenCallEquity(const Napi::CallbackInfo& info);
Napi::Value MultiwaySymmetricBreakevenCallEquityWithShare(const Napi::CallbackInfo& info);
Napi::Value TwoStreetPureBluffSameFoldEquity(const Napi::CallbackInfo& info);
Napi::Value TwoStreetPureBluffEv(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquitySecondStreetPureBluff(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquityFirstStreetPureBluff(const Napi::CallbackInfo& info);
Napi::Value ChubukovSymmetricJamBreakevenStack(const Napi::CallbackInfo& info);
Napi::Value ChubukovSymmetricJamEv(const Napi::CallbackInfo& info);
Napi::Value ChubukovMaxSymmetricJamStackChipsBinarySearch(const Napi::CallbackInfo& info);
Napi::Value IcmWinProbabilitiesHarville(const Napi::CallbackInfo& info);
Napi::Value IcmHarvillePlacementProbabilities(const Napi::CallbackInfo& info);
Napi::Value IcmTopKFinishProbabilities(const Napi::CallbackInfo& info);
Napi::Value IcmLastPlaceProbabilitiesHarville(const Napi::CallbackInfo& info);
Napi::Value IcmExpectedPayouts(const Napi::CallbackInfo& info);
Napi::Value IcmPairwiseBubbleFactor(const Napi::CallbackInfo& info);
Napi::Value SidePotLadderFromCommitments(const Napi::CallbackInfo& info);
Napi::Value LayeredPotChipEvFromEquities(const Napi::CallbackInfo& info);
Napi::Value SidePotLayersTotalChips(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsRandomHand(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsRandomHandAsync(const Napi::CallbackInfo& info);
Napi::Value StraightMadeFlopToRiverExactProbability(const Napi::CallbackInfo& info);
Napi::Value StraightMadeFlopToRiverExactProbabilityAsync(const Napi::CallbackInfo& info);
Napi::Value ChubukovMaxSymmetricJamStackBinarySearch(const Napi::CallbackInfo& info);
Napi::Value ChubukovMaxSymmetricJamStackFromHandBinarySearch(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeBatch(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeBatchPacked(const Napi::CallbackInfo& info);
Napi::Value EvaluateHandStrengthFastBatch(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsRandomHandBatch(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsKnownHand(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value SimulateEquityVsRange(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeDetailed(const Napi::CallbackInfo& info);
Napi::Value BuildPreflopEquityMatrix(const Napi::CallbackInfo& info);
Napi::Value EquityDeltaIfCardRemoved(const Napi::CallbackInfo& info);
Napi::Value IcmExpectedPayoutsWeitzman(const Napi::CallbackInfo& info);
Napi::Value FlopToTurnAtLeastOneHitProbability(const Napi::CallbackInfo& info);
Napi::Value TurnToRiverAtLeastOneHitProbability(const Napi::CallbackInfo& info);
Napi::Value FlopToTurnAtLeastOneHitUnionTwoCategories(const Napi::CallbackInfo& info);
Napi::Value TurnToRiverAtLeastOneHitUnionTwoCategories(const Napi::CallbackInfo& info);
Napi::Value FlopToTurnAtLeastOneHitUnionThreeCategories(const Napi::CallbackInfo& info);
Napi::Value TurnToRiverAtLeastOneHitUnionThreeCategories(const Napi::CallbackInfo& info);
Napi::Value FlopToTurnAtLeastOneHitUnionFourCategories(const Napi::CallbackInfo& info);
Napi::Value TurnToRiverAtLeastOneHitUnionFourCategories(const Napi::CallbackInfo& info);
Napi::Value FlopToTurnAtLeastOneHitDisjointOutsSum(const Napi::CallbackInfo& info);
Napi::Value TurnToRiverAtLeastOneHitDisjointOutsSum(const Napi::CallbackInfo& info);
Napi::Value HypergeometricTwoCardHitProbability(const Napi::CallbackInfo& info);
Napi::Value HypergeometricTwoCardMissProbability(const Napi::CallbackInfo& info);
Napi::Value RunnerRunnerBackdoorFlushOneCardProbability(const Napi::CallbackInfo& info);
Napi::Value BlockerAdjustedOuts(const Napi::CallbackInfo& info);
Napi::Value SuitBlockerFraction(const Napi::CallbackInfo& info);
Napi::Value NetPotAfterRake(const Napi::CallbackInfo& info);
Napi::Value NetPotAfterCallAndRake(const Napi::CallbackInfo& info);
Napi::Value EffectivePotOddsDisplayAfterRake(const Napi::CallbackInfo& info);
Napi::Value ImpliedBreakevenTotalPot(const Napi::CallbackInfo& info);
Napi::Value ImpliedOddsRequiredEquityFromFutureWin(const Napi::CallbackInfo& info);
Napi::Value ExpectedValueRaise(const Napi::CallbackInfo& info);
Napi::Value ExpectedValueRaiseWithRake(const Napi::CallbackInfo& info);
Napi::Value BreakevenRaiseEquity(const Napi::CallbackInfo& info);
Napi::Value BreakevenCallEquityWithPostedAnte(const Napi::CallbackInfo& info);
Napi::Value PotSizeAfterHuCall(const Napi::CallbackInfo& info);
Napi::Value PotSizeAfterHuBet(const Napi::CallbackInfo& info);
Napi::Value ExpectedValuePerBigBlind(const Napi::CallbackInfo& info);
Napi::Value MinimumDefenseFrequencyWithRake(const Napi::CallbackInfo& info);
Napi::Value AlphaFrequencyWithRake(const Napi::CallbackInfo& info);
Napi::Value BluffToValueRatioWithRake(const Napi::CallbackInfo& info);
Napi::Value ValueToBluffRatioWithRake(const Napi::CallbackInfo& info);
Napi::Value SprAfterBet(const Napi::CallbackInfo& info);
Napi::Value SprAfterRaise(const Napi::CallbackInfo& info);
Napi::Value CommitmentRatioAfterBet(const Napi::CallbackInfo& info);
Napi::Value BetSizeToMatchPotFraction(const Napi::CallbackInfo& info);
Napi::Value HalfKellyCriterionBinary(const Napi::CallbackInfo& info);
Napi::Value QuarterKellyCriterionBinary(const Napi::CallbackInfo& info);
Napi::Value EighthKellyCriterionBinary(const Napi::CallbackInfo& info);
Napi::Value KellyCriterionBinaryClamped(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquityPureBluffWithAnte(const Napi::CallbackInfo& info);
Napi::Value BreakevenFoldEquitySemiBluffWithAnte(const Napi::CallbackInfo& info);
Napi::Value TwoStreetPureBluffEvWithRake(const Napi::CallbackInfo& info);
Napi::Value ThreeStreetPureBluffSameFoldEquity(const Napi::CallbackInfo& info);
Napi::Value ThreeStreetPureBluffEv(const Napi::CallbackInfo& info);
Napi::Value MultiwaySymmetricBreakevenCallEquityWithRake(const Napi::CallbackInfo& info);
Napi::Value MultiwaySymmetricBreakevenCallEquityWithShareAndRake(const Napi::CallbackInfo& info);
Napi::Value MultiwayExpectedValueCall(const Napi::CallbackInfo& info);
Napi::Value ReverseImpliedOddsMinEquity(const Napi::CallbackInfo& info);
Napi::Value GeometricPotAfterSingleMatchedBet(const Napi::CallbackInfo& info);
Napi::Value BinomialProportionCiWidth(const Napi::CallbackInfo& info);
Napi::Value MonteCarloTrialsForWilsonHalfWidth(const Napi::CallbackInfo& info);
Napi::Value VarianceToStandardDeviationPerHand(const Napi::CallbackInfo& info);
Napi::Value IcmEqualChopPayouts(const Napi::CallbackInfo& info);
Napi::Value IcmChopSurplusVsEqualSplit(const Napi::CallbackInfo& info);
Napi::Value IcmTotalPrizePool(const Napi::CallbackInfo& info);
Napi::Value IcmDealEvPerChip(const Napi::CallbackInfo& info);
Napi::Value IcmSatelliteAdvanceProbability(const Napi::CallbackInfo& info);
Napi::Value IcmPayoutStructureGini(const Napi::CallbackInfo& info);
Napi::Value IcmChipLeaderPremiumVsEqualChop(const Napi::CallbackInfo& info);
Napi::Value SidePotLayerCount(const Napi::CallbackInfo& info);
Napi::Value SidePotBreakevenCallEquity(const Napi::CallbackInfo& info);
Napi::Value PreflopCombosFromNotationMinusBlockers(const Napi::CallbackInfo& info);
Napi::Value StackToPotAfterCall(const Napi::CallbackInfo& info);
Napi::Value FlushMadeFlopToRiverExactProbability(const Napi::CallbackInfo& info);
Napi::Value FlushMadeFlopToRiverExactProbabilityAsync(const Napi::CallbackInfo& info);
Napi::Value FullHouseMadeFlopToRiverExactProbability(const Napi::CallbackInfo& info);
Napi::Value FullHouseMadeFlopToRiverExactProbabilityAsync(const Napi::CallbackInfo& info);
Napi::Value TripsMadeFlopToRiverExactProbability(const Napi::CallbackInfo& info);
Napi::Value TripsMadeFlopToRiverExactProbabilityAsync(const Napi::CallbackInfo& info);
Napi::Value TwoPairMadeFlopToRiverExactProbability(const Napi::CallbackInfo& info);
Napi::Value TwoPairMadeFlopToRiverExactProbabilityAsync(const Napi::CallbackInfo& info);
Napi::Value ExactHeroCategoryAtLeastFlopToRiver(const Napi::CallbackInfo& info);
Napi::Value ExactHeroCategoryAtLeastFlopToRiverAsync(const Napi::CallbackInfo& info);
Napi::Value PushFoldSymmetricEv(const Napi::CallbackInfo& info);
Napi::Value PushFoldSymmetricBreakevenEquity(const Napi::CallbackInfo& info);
Napi::Value OpenRaiseBreakevenFoldEquity(const Napi::CallbackInfo& info);
Napi::Value CallOrFoldChipEvDelta(const Napi::CallbackInfo& info);
Napi::Value NormalizedRangeWeightSum(const Napi::CallbackInfo& info);
Napi::Value LayeredPotChipEvFromEquitiesWithRake(const Napi::CallbackInfo& info);
Napi::Value IcmExpectedPayoutsDeltaFromChipChop(const Napi::CallbackInfo& info);

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    poker_bind::init_binding(env);
    exports.DefineProperties({
        Napi::PropertyDescriptor::Function("evaluateBestHand", EvaluateBestHand),
        Napi::PropertyDescriptor::Function("evaluateHandStrength", EvaluateHandStrength),
        Napi::PropertyDescriptor::Function("evaluateHandStrengthFast", EvaluateHandStrengthFast),
        Napi::PropertyDescriptor::Function("benchmarkEvaluatorThroughput", BenchmarkEvaluatorThroughput),
        Napi::PropertyDescriptor::Function("benchmarkEvaluatorThroughputAsync", BenchmarkEvaluatorThroughputAsync),
        Napi::PropertyDescriptor::Function("evaluateHandCategory", EvaluateHandCategory),
        Napi::PropertyDescriptor::Function("validateCardString", ValidateCardString),
        Napi::PropertyDescriptor::Function("cardStringsHaveDuplicate", CardStringsHaveDuplicate),
        Napi::PropertyDescriptor::Function("canonicalCardString", CanonicalCardString),
        Napi::PropertyDescriptor::Function("parseCompactCardList", ParseCompactCardList),
        Napi::PropertyDescriptor::Function("compareBestHands", CompareBestHands),
        Napi::PropertyDescriptor::Function("simulateHandOutcome", SimulateHandOutcome),
        Napi::PropertyDescriptor::Function("simulateHandOutcomeAsync", SimulateHandOutcomeAsync),
        Napi::PropertyDescriptor::Function("parallelHandSimulation", ParallelHandSimulation),
        Napi::PropertyDescriptor::Function("parallelHandSimulationAsync", ParallelHandSimulationAsync),
        Napi::PropertyDescriptor::Function("decideAction", DecideAction),
        Napi::PropertyDescriptor::Function("decideActionAsync", DecideActionAsync),
        Napi::PropertyDescriptor::Function("potOddsRatio", PotOddsRatio),
        Napi::PropertyDescriptor::Function("expectedValueCall", ExpectedValueCall),
        Napi::PropertyDescriptor::Function("expectedValueCallWithRake", ExpectedValueCallWithRake),
        Napi::PropertyDescriptor::Function("spr", Spr),
        Napi::PropertyDescriptor::Function("effectiveStack", EffectiveStack),
        Napi::PropertyDescriptor::Function("normalizedStackFractions", NormalizedStackFractions),
        Napi::PropertyDescriptor::Function("breakevenCallEquity", BreakevenCallEquity),
        Napi::PropertyDescriptor::Function("minimumDefenseFrequency", MinimumDefenseFrequency),
        Napi::PropertyDescriptor::Function("stackInBigBlinds", StackInBigBlinds),
        Napi::PropertyDescriptor::Function("potOddsRatioDisplay", PotOddsRatioDisplay),
        Napi::PropertyDescriptor::Function("formatPotOdds", FormatPotOdds),
        Napi::PropertyDescriptor::Function("breakevenCallEquityFromPotOddsDisplayRatio", BreakevenCallEquityFromPotOddsDisplayRatio),
        Napi::PropertyDescriptor::Function("potOddsDisplayRatioFromBreakevenCallEquity", PotOddsDisplayRatioFromBreakevenCallEquity),
        Napi::PropertyDescriptor::Function("formatPotOddsReducedFraction", FormatPotOddsReducedFraction),
        Napi::PropertyDescriptor::Function("equityToWinningOddsAgainst", EquityToWinningOddsAgainst),
        Napi::PropertyDescriptor::Function("winningOddsAgainstToEquity", WinningOddsAgainstToEquity),
        Napi::PropertyDescriptor::Function("ruleOfFourEquity", RuleOfFourEquity),
        Napi::PropertyDescriptor::Function("ruleOfTwoEquity", RuleOfTwoEquity),
        Napi::PropertyDescriptor::Function("estimatedOutsFromRuleOfTwo", EstimatedOutsFromRuleOfTwo),
        Napi::PropertyDescriptor::Function("estimatedOutsFromRuleOfFour", EstimatedOutsFromRuleOfFour),
        Napi::PropertyDescriptor::Function("impliedBreakevenFutureWin", ImpliedBreakevenFutureWin),
        Napi::PropertyDescriptor::Function("bluffToValueRatio", BluffToValueRatio),
        Napi::PropertyDescriptor::Function("valueToBluffRatio", ValueToBluffRatio),
        Napi::PropertyDescriptor::Function("betAsPotFraction", BetAsPotFraction),
        Napi::PropertyDescriptor::Function("sprAfterCall", SprAfterCall),
        Napi::PropertyDescriptor::Function("commitmentRatio", CommitmentRatio),
        Napi::PropertyDescriptor::Function("alphaFrequency", AlphaFrequency),
        Napi::PropertyDescriptor::Function("breakevenFoldEquityPureBluff", BreakevenFoldEquityPureBluff),
        Napi::PropertyDescriptor::Function("breakevenFoldEquitySemiBluff", BreakevenFoldEquitySemiBluff),
        Napi::PropertyDescriptor::Function("hypergeometricOneCardHitProbability", HypergeometricOneCardHitProbability),
        Napi::PropertyDescriptor::Function("runnerRunnerBackdoorFlushTwoCardProbability", RunnerRunnerBackdoorFlushTwoCardProbability),
        Napi::PropertyDescriptor::Function("flopToRiverAtLeastOneHitProbability", FlopToRiverAtLeastOneHitProbability),
        Napi::PropertyDescriptor::Function("flopToRiverAtLeastOneHitUnionTwoCategories", FlopToRiverAtLeastOneHitUnionTwoCategories),
        Napi::PropertyDescriptor::Function("flopToRiverAtLeastOneHitUnionThreeCategories", FlopToRiverAtLeastOneHitUnionThreeCategories),
        Napi::PropertyDescriptor::Function("flopToRiverAtLeastOneHitUnionFourCategories", FlopToRiverAtLeastOneHitUnionFourCategories),
        Napi::PropertyDescriptor::Function("flopToRiverAtLeastOneHitDisjointOutsSum", FlopToRiverAtLeastOneHitDisjointOutsSum),
        Napi::PropertyDescriptor::Function("runnerRunnerStraightDrawHitProbability", RunnerRunnerStraightDrawHitProbability),
        Napi::PropertyDescriptor::Function("reverseImpliedOddsMaxFutureLoss", ReverseImpliedOddsMaxFutureLoss),
        Napi::PropertyDescriptor::Function("geometricPotAfterMatchedPotFractions", GeometricPotAfterMatchedPotFractions),
        Napi::PropertyDescriptor::Function("harringtonM", HarringtonM),
        Napi::PropertyDescriptor::Function("harringtonMEffective", HarringtonMEffective),
        Napi::PropertyDescriptor::Function("harringtonMEffectiveActiveAntes", HarringtonMEffectiveActiveAntes),
        Napi::PropertyDescriptor::Function("harringtonQ", HarringtonQ),
        Napi::PropertyDescriptor::Function("orbitCostChips", OrbitCostChips),
        Napi::PropertyDescriptor::Function("nlMinimumRaiseToTotal", NlMinimumRaiseToTotal),
        Napi::PropertyDescriptor::Function("preflopCombosFromNotation", PreflopCombosFromNotation),
        Napi::PropertyDescriptor::Function("preflopCombosFromNotationsList", PreflopCombosFromNotationsList),
        Napi::PropertyDescriptor::Function("handRankCategoryOrder", HandRankCategoryOrder),
        Napi::PropertyDescriptor::Function("kellyCriterionBinary", KellyCriterionBinary),
        Napi::PropertyDescriptor::Function("monteCarloStandardError", MonteCarloStandardError),
        Napi::PropertyDescriptor::Function("monteCarloTrialsForStandardErrorBound", MonteCarloTrialsForStandardErrorBound),
        Napi::PropertyDescriptor::Function("betaBinomialFoldPosterior", BetaBinomialFoldPosterior),
        Napi::PropertyDescriptor::Function("duplicationAdjustedOuts", DuplicationAdjustedOuts),
        Napi::PropertyDescriptor::Function("riskOfRuinDiffusionApprox", RiskOfRuinDiffusionApprox),
        Napi::PropertyDescriptor::Function("bankrollForTargetRorDiffusion", BankrollForTargetRorDiffusion),
        Napi::PropertyDescriptor::Function("wilsonScoreInterval", WilsonScoreInterval),
        Napi::PropertyDescriptor::Function("agrestiCoullInterval", AgrestiCoullInterval),
        Napi::PropertyDescriptor::Function("normalWaldBinomialInterval", NormalWaldBinomialInterval),
        Napi::PropertyDescriptor::Function("monteCarloTrialsForHoeffdingBound", MonteCarloTrialsForHoeffdingBound),
        Napi::PropertyDescriptor::Function("rakeFromPot", RakeFromPot),
        Napi::PropertyDescriptor::Function("breakevenCallEquityWithRake", BreakevenCallEquityWithRake),
        Napi::PropertyDescriptor::Function("breakevenFoldEquitySemiBluffWithRake", BreakevenFoldEquitySemiBluffWithRake),
        Napi::PropertyDescriptor::Function("breakevenFoldEquityPureBluffWithRake", BreakevenFoldEquityPureBluffWithRake),
        Napi::PropertyDescriptor::Function("multiwaySymmetricBreakevenCallEquity", MultiwaySymmetricBreakevenCallEquity),
        Napi::PropertyDescriptor::Function("multiwaySymmetricBreakevenCallEquityWithShare", MultiwaySymmetricBreakevenCallEquityWithShare),
        Napi::PropertyDescriptor::Function("twoStreetPureBluffSameFoldEquity", TwoStreetPureBluffSameFoldEquity),
        Napi::PropertyDescriptor::Function("twoStreetPureBluffEv", TwoStreetPureBluffEv),
        Napi::PropertyDescriptor::Function("breakevenFoldEquitySecondStreetPureBluff", BreakevenFoldEquitySecondStreetPureBluff),
        Napi::PropertyDescriptor::Function("breakevenFoldEquityFirstStreetPureBluff", BreakevenFoldEquityFirstStreetPureBluff),
        Napi::PropertyDescriptor::Function("chubukovSymmetricJamBreakevenStack", ChubukovSymmetricJamBreakevenStack),
        Napi::PropertyDescriptor::Function("chubukovSymmetricJamEv", ChubukovSymmetricJamEv),
        Napi::PropertyDescriptor::Function("chubukovMaxSymmetricJamStackChipsBinarySearch", ChubukovMaxSymmetricJamStackChipsBinarySearch),
        Napi::PropertyDescriptor::Function("icmWinProbabilitiesHarville", IcmWinProbabilitiesHarville),
        Napi::PropertyDescriptor::Function("icmHarvillePlacementProbabilities", IcmHarvillePlacementProbabilities),
        Napi::PropertyDescriptor::Function("icmTopKFinishProbabilities", IcmTopKFinishProbabilities),
        Napi::PropertyDescriptor::Function("icmLastPlaceProbabilitiesHarville", IcmLastPlaceProbabilitiesHarville),
        Napi::PropertyDescriptor::Function("icmExpectedPayouts", IcmExpectedPayouts),
        Napi::PropertyDescriptor::Function("icmPairwiseBubbleFactor", IcmPairwiseBubbleFactor),
        Napi::PropertyDescriptor::Function("sidePotLadderFromCommitments", SidePotLadderFromCommitments),
        Napi::PropertyDescriptor::Function("layeredPotChipEvFromEquities", LayeredPotChipEvFromEquities),
        Napi::PropertyDescriptor::Function("sidePotLayersTotalChips", SidePotLayersTotalChips),
        Napi::PropertyDescriptor::Function("exactHuEquityVsRandomHand", ExactHuEquityVsRandomHand),
        Napi::PropertyDescriptor::Function("exactHuEquityVsRandomHandAsync", ExactHuEquityVsRandomHandAsync),
        Napi::PropertyDescriptor::Function("straightMadeFlopToRiverExactProbability", StraightMadeFlopToRiverExactProbability),
        Napi::PropertyDescriptor::Function("straightMadeFlopToRiverExactProbabilityAsync", StraightMadeFlopToRiverExactProbabilityAsync),
        Napi::PropertyDescriptor::Function("chubukovMaxSymmetricJamStackBinarySearch", ChubukovMaxSymmetricJamStackBinarySearch),
        Napi::PropertyDescriptor::Function("chubukovMaxSymmetricJamStackFromHandBinarySearch", ChubukovMaxSymmetricJamStackFromHandBinarySearch),
        Napi::PropertyDescriptor::Function("simulateHandOutcomeBatch", SimulateHandOutcomeBatch),
        Napi::PropertyDescriptor::Function("simulateHandOutcomeBatchPacked", SimulateHandOutcomeBatchPacked),
        Napi::PropertyDescriptor::Function("evaluateHandStrengthFastBatch", EvaluateHandStrengthFastBatch),
        Napi::PropertyDescriptor::Function("exactHuEquityVsRandomHandBatch", ExactHuEquityVsRandomHandBatch),
        Napi::PropertyDescriptor::Function("exactHuEquityVsKnownHand", ExactHuEquityVsKnownHand),
        Napi::PropertyDescriptor::Function("exactHuEquityVsRange", ExactHuEquityVsRange),
        Napi::PropertyDescriptor::Function("simulateEquityVsRange", SimulateEquityVsRange),
        Napi::PropertyDescriptor::Function("simulateHandOutcomeDetailed", SimulateHandOutcomeDetailed),
        Napi::PropertyDescriptor::Function("buildPreflopEquityMatrix", BuildPreflopEquityMatrix),
        Napi::PropertyDescriptor::Function("equityDeltaIfCardRemoved", EquityDeltaIfCardRemoved),
        Napi::PropertyDescriptor::Function("icmExpectedPayoutsWeitzman", IcmExpectedPayoutsWeitzman),
        Napi::PropertyDescriptor::Function("encodePokerState", poker_bind::EncodePokerState),
        Napi::PropertyDescriptor::Function("decodePokerState", poker_bind::DecodePokerState),
        Napi::PropertyDescriptor::Function("icmShapleyValues", IcmShapleyValues),
        Napi::PropertyDescriptor::Function("icmHarvilleStackJacobian", IcmHarvilleStackJacobian),
        Napi::PropertyDescriptor::Function("icmHarvilleSkillAdjustedPayouts", IcmHarvilleSkillAdjustedPayouts),
        Napi::PropertyDescriptor::Function("icmFieldPressureIndex", IcmFieldPressureIndex),
        Napi::PropertyDescriptor::Function("icmChopNegotiationAnalysis", IcmChopNegotiationAnalysis),
        Napi::PropertyDescriptor::Function("tournamentDuelAbsorptionProbabilities",
                                          TournamentDuelAbsorptionProbabilities),
        Napi::PropertyDescriptor::Function("sidePotLayerTournamentEvDelta", SidePotLayerTournamentEvDelta),
        Napi::PropertyDescriptor::Function("materializeVillainRangeAfterBlockers",
                                          MaterializeVillainRangeAfterBlockers),
        Napi::PropertyDescriptor::Function("bayesianRangeUpdateFromAction", BayesianRangeUpdateFromAction),
        Napi::PropertyDescriptor::Function("solveRiverPolarizedIndifferenceBet", SolveRiverPolarizedIndifferenceBet),
        Napi::PropertyDescriptor::Function("solveStageMinimaxRegretBet", SolveStageMinimaxRegretBet),
        Napi::PropertyDescriptor::Function("exactInformationRegretVsClairvoyant",
                                          ExactInformationRegretVsClairvoyant),
        Napi::PropertyDescriptor::Function("multiwayEquityIndependenceGap", MultiwayEquityIndependenceGap),
        Napi::PropertyDescriptor::Function("solveSymmetricPushFoldThreshold", SolveSymmetricPushFoldThreshold),
        Napi::PropertyDescriptor::Function("exactHeroRunoutVulnerability", ExactHeroRunoutVulnerability),
        Napi::PropertyDescriptor::Function("exactHeroRunoutVulnerabilityAsync", ExactHeroRunoutVulnerabilityAsync),
        Napi::PropertyDescriptor::Function("exactVillainLeapfrogOutCounts", ExactVillainLeapfrogOutCounts),
        Napi::PropertyDescriptor::Function("exactHeroCategoryJointFlopToRiver",
                                          ExactHeroCategoryJointFlopToRiver),
        Napi::PropertyDescriptor::Function("exactRangeDominatedComboFraction", ExactRangeDominatedComboFraction),
        Napi::PropertyDescriptor::Function("exactHeroEquityRunoutQuantiles", ExactHeroEquityRunoutQuantiles),
        Napi::PropertyDescriptor::Function("exactHeroEquityRunoutQuantilesAsync",
                                          ExactHeroEquityRunoutQuantilesAsync),
        Napi::PropertyDescriptor::Function("exactEquityCardRemovalGradient", ExactEquityCardRemovalGradient),
        Napi::PropertyDescriptor::Function("exactEquityCardRemovalGradientAsync",
                                          ExactEquityCardRemovalGradientAsync),
        Napi::PropertyDescriptor::Function("flopToTurnAtLeastOneHitProbability", FlopToTurnAtLeastOneHitProbability),
        Napi::PropertyDescriptor::Function("turnToRiverAtLeastOneHitProbability", TurnToRiverAtLeastOneHitProbability),
        Napi::PropertyDescriptor::Function("flopToTurnAtLeastOneHitUnionTwoCategories", FlopToTurnAtLeastOneHitUnionTwoCategories),
        Napi::PropertyDescriptor::Function("turnToRiverAtLeastOneHitUnionTwoCategories", TurnToRiverAtLeastOneHitUnionTwoCategories),
        Napi::PropertyDescriptor::Function("flopToTurnAtLeastOneHitUnionThreeCategories", FlopToTurnAtLeastOneHitUnionThreeCategories),
        Napi::PropertyDescriptor::Function("turnToRiverAtLeastOneHitUnionThreeCategories", TurnToRiverAtLeastOneHitUnionThreeCategories),
        Napi::PropertyDescriptor::Function("flopToTurnAtLeastOneHitUnionFourCategories", FlopToTurnAtLeastOneHitUnionFourCategories),
        Napi::PropertyDescriptor::Function("turnToRiverAtLeastOneHitUnionFourCategories", TurnToRiverAtLeastOneHitUnionFourCategories),
        Napi::PropertyDescriptor::Function("flopToTurnAtLeastOneHitDisjointOutsSum", FlopToTurnAtLeastOneHitDisjointOutsSum),
        Napi::PropertyDescriptor::Function("turnToRiverAtLeastOneHitDisjointOutsSum", TurnToRiverAtLeastOneHitDisjointOutsSum),
        Napi::PropertyDescriptor::Function("hypergeometricTwoCardHitProbability", HypergeometricTwoCardHitProbability),
        Napi::PropertyDescriptor::Function("hypergeometricTwoCardMissProbability", HypergeometricTwoCardMissProbability),
        Napi::PropertyDescriptor::Function("runnerRunnerBackdoorFlushOneCardProbability", RunnerRunnerBackdoorFlushOneCardProbability),
        Napi::PropertyDescriptor::Function("blockerAdjustedOuts", BlockerAdjustedOuts),
        Napi::PropertyDescriptor::Function("suitBlockerFraction", SuitBlockerFraction),
        Napi::PropertyDescriptor::Function("netPotAfterRake", NetPotAfterRake),
        Napi::PropertyDescriptor::Function("netPotAfterCallAndRake", NetPotAfterCallAndRake),
        Napi::PropertyDescriptor::Function("effectivePotOddsDisplayAfterRake", EffectivePotOddsDisplayAfterRake),
        Napi::PropertyDescriptor::Function("impliedBreakevenTotalPot", ImpliedBreakevenTotalPot),
        Napi::PropertyDescriptor::Function("impliedOddsRequiredEquityFromFutureWin", ImpliedOddsRequiredEquityFromFutureWin),
        Napi::PropertyDescriptor::Function("expectedValueRaise", ExpectedValueRaise),
        Napi::PropertyDescriptor::Function("expectedValueRaiseWithRake", ExpectedValueRaiseWithRake),
        Napi::PropertyDescriptor::Function("breakevenRaiseEquity", BreakevenRaiseEquity),
        Napi::PropertyDescriptor::Function("breakevenCallEquityWithPostedAnte", BreakevenCallEquityWithPostedAnte),
        Napi::PropertyDescriptor::Function("potSizeAfterHuCall", PotSizeAfterHuCall),
        Napi::PropertyDescriptor::Function("potSizeAfterHuBet", PotSizeAfterHuBet),
        Napi::PropertyDescriptor::Function("expectedValuePerBigBlind", ExpectedValuePerBigBlind),
        Napi::PropertyDescriptor::Function("minimumDefenseFrequencyWithRake", MinimumDefenseFrequencyWithRake),
        Napi::PropertyDescriptor::Function("alphaFrequencyWithRake", AlphaFrequencyWithRake),
        Napi::PropertyDescriptor::Function("bluffToValueRatioWithRake", BluffToValueRatioWithRake),
        Napi::PropertyDescriptor::Function("valueToBluffRatioWithRake", ValueToBluffRatioWithRake),
        Napi::PropertyDescriptor::Function("sprAfterBet", SprAfterBet),
        Napi::PropertyDescriptor::Function("sprAfterRaise", SprAfterRaise),
        Napi::PropertyDescriptor::Function("commitmentRatioAfterBet", CommitmentRatioAfterBet),
        Napi::PropertyDescriptor::Function("betSizeToMatchPotFraction", BetSizeToMatchPotFraction),
        Napi::PropertyDescriptor::Function("halfKellyCriterionBinary", HalfKellyCriterionBinary),
        Napi::PropertyDescriptor::Function("quarterKellyCriterionBinary", QuarterKellyCriterionBinary),
        Napi::PropertyDescriptor::Function("eighthKellyCriterionBinary", EighthKellyCriterionBinary),
        Napi::PropertyDescriptor::Function("kellyCriterionBinaryClamped", KellyCriterionBinaryClamped),
        Napi::PropertyDescriptor::Function("breakevenFoldEquityPureBluffWithAnte", BreakevenFoldEquityPureBluffWithAnte),
        Napi::PropertyDescriptor::Function("breakevenFoldEquitySemiBluffWithAnte", BreakevenFoldEquitySemiBluffWithAnte),
        Napi::PropertyDescriptor::Function("twoStreetPureBluffEvWithRake", TwoStreetPureBluffEvWithRake),
        Napi::PropertyDescriptor::Function("threeStreetPureBluffSameFoldEquity", ThreeStreetPureBluffSameFoldEquity),
        Napi::PropertyDescriptor::Function("threeStreetPureBluffEv", ThreeStreetPureBluffEv),
        Napi::PropertyDescriptor::Function("multiwaySymmetricBreakevenCallEquityWithRake", MultiwaySymmetricBreakevenCallEquityWithRake),
        Napi::PropertyDescriptor::Function("multiwaySymmetricBreakevenCallEquityWithShareAndRake", MultiwaySymmetricBreakevenCallEquityWithShareAndRake),
        Napi::PropertyDescriptor::Function("multiwayExpectedValueCall", MultiwayExpectedValueCall),
        Napi::PropertyDescriptor::Function("reverseImpliedOddsMinEquity", ReverseImpliedOddsMinEquity),
        Napi::PropertyDescriptor::Function("geometricPotAfterSingleMatchedBet", GeometricPotAfterSingleMatchedBet),
        Napi::PropertyDescriptor::Function("binomialProportionCiWidth", BinomialProportionCiWidth),
        Napi::PropertyDescriptor::Function("monteCarloTrialsForWilsonHalfWidth", MonteCarloTrialsForWilsonHalfWidth),
        Napi::PropertyDescriptor::Function("varianceToStandardDeviationPerHand", VarianceToStandardDeviationPerHand),
        Napi::PropertyDescriptor::Function("icmEqualChopPayouts", IcmEqualChopPayouts),
        Napi::PropertyDescriptor::Function("icmChopSurplusVsEqualSplit", IcmChopSurplusVsEqualSplit),
        Napi::PropertyDescriptor::Function("icmTotalPrizePool", IcmTotalPrizePool),
        Napi::PropertyDescriptor::Function("icmDealEvPerChip", IcmDealEvPerChip),
        Napi::PropertyDescriptor::Function("icmSatelliteAdvanceProbability", IcmSatelliteAdvanceProbability),
        Napi::PropertyDescriptor::Function("icmPayoutStructureGini", IcmPayoutStructureGini),
        Napi::PropertyDescriptor::Function("icmChipLeaderPremiumVsEqualChop", IcmChipLeaderPremiumVsEqualChop),
        Napi::PropertyDescriptor::Function("sidePotLayerCount", SidePotLayerCount),
        Napi::PropertyDescriptor::Function("sidePotBreakevenCallEquity", SidePotBreakevenCallEquity),
        Napi::PropertyDescriptor::Function("preflopCombosFromNotationMinusBlockers", PreflopCombosFromNotationMinusBlockers),
        Napi::PropertyDescriptor::Function("stackToPotAfterCall", StackToPotAfterCall),
        Napi::PropertyDescriptor::Function("flushMadeFlopToRiverExactProbability", FlushMadeFlopToRiverExactProbability),
        Napi::PropertyDescriptor::Function("flushMadeFlopToRiverExactProbabilityAsync", FlushMadeFlopToRiverExactProbabilityAsync),
        Napi::PropertyDescriptor::Function("fullHouseMadeFlopToRiverExactProbability", FullHouseMadeFlopToRiverExactProbability),
        Napi::PropertyDescriptor::Function("fullHouseMadeFlopToRiverExactProbabilityAsync", FullHouseMadeFlopToRiverExactProbabilityAsync),
        Napi::PropertyDescriptor::Function("tripsMadeFlopToRiverExactProbability", TripsMadeFlopToRiverExactProbability),
        Napi::PropertyDescriptor::Function("tripsMadeFlopToRiverExactProbabilityAsync", TripsMadeFlopToRiverExactProbabilityAsync),
        Napi::PropertyDescriptor::Function("twoPairMadeFlopToRiverExactProbability", TwoPairMadeFlopToRiverExactProbability),
        Napi::PropertyDescriptor::Function("twoPairMadeFlopToRiverExactProbabilityAsync", TwoPairMadeFlopToRiverExactProbabilityAsync),
        Napi::PropertyDescriptor::Function("exactHeroCategoryAtLeastFlopToRiver", ExactHeroCategoryAtLeastFlopToRiver),
        Napi::PropertyDescriptor::Function("exactHeroCategoryAtLeastFlopToRiverAsync", ExactHeroCategoryAtLeastFlopToRiverAsync),
        Napi::PropertyDescriptor::Function("pushFoldSymmetricEv", PushFoldSymmetricEv),
        Napi::PropertyDescriptor::Function("pushFoldSymmetricBreakevenEquity", PushFoldSymmetricBreakevenEquity),
        Napi::PropertyDescriptor::Function("openRaiseBreakevenFoldEquity", OpenRaiseBreakevenFoldEquity),
        Napi::PropertyDescriptor::Function("callOrFoldChipEvDelta", CallOrFoldChipEvDelta),
        Napi::PropertyDescriptor::Function("normalizedRangeWeightSum", NormalizedRangeWeightSum),
        Napi::PropertyDescriptor::Function("layeredPotChipEvFromEquitiesWithRake", LayeredPotChipEvFromEquitiesWithRake),
        Napi::PropertyDescriptor::Function("icmExpectedPayoutsDeltaFromChipChop", IcmExpectedPayoutsDeltaFromChipChop),
        Napi::PropertyDescriptor::Function("normalizeSparseRange", NormalizeSparseRange),
        Napi::PropertyDescriptor::Function("pruneRangeByMinWeight", PruneRangeByMinWeight),
        Napi::PropertyDescriptor::Function("mergeSparseRanges", MergeSparseRanges),
        Napi::PropertyDescriptor::Function("intersectSparseRanges", IntersectSparseRanges),
        Napi::PropertyDescriptor::Function("subtractSparseRange", SubtractSparseRange),
        Napi::PropertyDescriptor::Function("rangeComboCount", RangeComboCount),
        Napi::PropertyDescriptor::Function("rangeShannonEntropy", RangeShannonEntropy),
        Napi::PropertyDescriptor::Function("rangeGiniCoefficient", RangeGiniCoefficient),
        Napi::PropertyDescriptor::Function("rangeCoverageFraction", RangeCoverageFraction),
        Napi::PropertyDescriptor::Function("rangeWeightTopKMass", RangeWeightTopKMass),
        Napi::PropertyDescriptor::Function("rangeDistanceL1", RangeDistanceL1),
        Napi::PropertyDescriptor::Function("rangeDistanceL2", RangeDistanceL2),
        Napi::PropertyDescriptor::Function("rangeDistanceJensenShannon", RangeDistanceJensenShannon),
        Napi::PropertyDescriptor::Function("rangeCosineSimilarity", RangeCosineSimilarity),
        Napi::PropertyDescriptor::Function("rangeTopCombos", RangeTopCombos),
        Napi::PropertyDescriptor::Function("rangeBucketWeightsByHandClass", RangeBucketWeightsByHandClass),
        Napi::PropertyDescriptor::Function("rangeBucketWeightsByNotation", RangeBucketWeightsByNotation),
        Napi::PropertyDescriptor::Function("rangeFromNotationWeights", RangeFromNotationWeights),
        Napi::PropertyDescriptor::Function("rangeBlockerPressureByCard", RangeBlockerPressureByCard),
        Napi::PropertyDescriptor::Function("rangeRemovalSensitivityVsHero", RangeRemovalSensitivityVsHero),
        Napi::PropertyDescriptor::Function("classifyBoardTexture", ClassifyBoardTexture),
        Napi::PropertyDescriptor::Function("boardTextureScore", BoardTextureScore),
        Napi::PropertyDescriptor::Function("boardWetnessScore", BoardWetnessScore),
        Napi::PropertyDescriptor::Function("boardPairednessIndex", BoardPairednessIndex),
        Napi::PropertyDescriptor::Function("boardFlushPressure", BoardFlushPressure),
        Napi::PropertyDescriptor::Function("boardStraightPressure", BoardStraightPressure),
        Napi::PropertyDescriptor::Function("boardNutAdvantageApprox", BoardNutAdvantageApprox),
        Napi::PropertyDescriptor::Function("boardRangeInteractionScore", BoardRangeInteractionScore),
        Napi::PropertyDescriptor::Function("boardStaticnessIndex", BoardStaticnessIndex),
        Napi::PropertyDescriptor::Function("boardTurnVolatility", BoardTurnVolatility),
        Napi::PropertyDescriptor::Function("boardRiverScareCardScore", BoardRiverScareCardScore),
        Napi::PropertyDescriptor::Function("enumerateScareCards", EnumerateScareCards),
        Napi::PropertyDescriptor::Function("boardEquityShiftDistribution", BoardEquityShiftDistribution),
        Napi::PropertyDescriptor::Function("rangeBoardCoverage", RangeBoardCoverage),
        Napi::PropertyDescriptor::Function("heroBoardConnectivityScore", HeroBoardConnectivityScore),
        Napi::PropertyDescriptor::Function("blockerMatrixByCard", BlockerMatrixByCard),
        Napi::PropertyDescriptor::Function("exactEquityDistributionVsRange", ExactEquityDistributionVsRange),
        Napi::PropertyDescriptor::Function("exactEquityPercentileVsRange", ExactEquityPercentileVsRange),
        Napi::PropertyDescriptor::Function("exactEquityRealizationEstimate", ExactEquityRealizationEstimate),
        Napi::PropertyDescriptor::Function("equityRealizationPenalty", EquityRealizationPenalty),
        Napi::PropertyDescriptor::Function("riverCallThresholdDistribution", RiverCallThresholdDistribution),
        Napi::PropertyDescriptor::Function("turnBarrelRunoutEvDistribution", TurnBarrelRunoutEvDistribution),
        Napi::PropertyDescriptor::Function("delayedCbetRunoutScore", DelayedCbetRunoutScore),
        Napi::PropertyDescriptor::Function("protectionBetBenefit", ProtectionBetBenefit),
        Napi::PropertyDescriptor::Function("equityDenialValue", EquityDenialValue),
        Napi::PropertyDescriptor::Function("showdownValueIndex", ShowdownValueIndex),
        Napi::PropertyDescriptor::Function("cbetSizeEvGrid", CbetSizeEvGrid),
        Napi::PropertyDescriptor::Function("probeBetEvGrid", ProbeBetEvGrid),
        Napi::PropertyDescriptor::Function("checkRaiseSemiBluffEv", CheckRaiseSemiBluffEv),
        Napi::PropertyDescriptor::Function("overbetPolarizationScore", OverbetPolarizationScore),
        Napi::PropertyDescriptor::Function("geometricStreetSizingPlan", GeometricStreetSizingPlan),
        Napi::PropertyDescriptor::Function("riverValueBetThreshold", RiverValueBetThreshold),
        Napi::PropertyDescriptor::Function("riverBluffCandidateScore", RiverBluffCandidateScore),
        Napi::PropertyDescriptor::Function("thinValueMargin", ThinValueMargin),
        Napi::PropertyDescriptor::Function("betSizingIndifferencePoint", BetSizingIndifferencePoint),
        Napi::PropertyDescriptor::Function("multiStreetStackOffThreshold", MultiStreetStackOffThreshold),
        Napi::PropertyDescriptor::Function("foldEquityNeededByStreetPlan", FoldEquityNeededByStreetPlan),
        Napi::PropertyDescriptor::Function("bluffCatchDecisionScore", BluffCatchDecisionScore),
        Napi::PropertyDescriptor::Function("blockerAwareBluffFrequency", BlockerAwareBluffFrequency),
        Napi::PropertyDescriptor::Function("valueTargetingScore", ValueTargetingScore),
        Napi::PropertyDescriptor::Function("opponentFoldToCbetPosterior", OpponentFoldToCbetPosterior),
        Napi::PropertyDescriptor::Function("opponentAggressionFactor", OpponentAggressionFactor),
        Napi::PropertyDescriptor::Function("opponentShowdownBiasEstimate", OpponentShowdownBiasEstimate),
        Napi::PropertyDescriptor::Function("opponentRangeElasticityFromSizing", OpponentRangeElasticityFromSizing),
        Napi::PropertyDescriptor::Function("exploitativeBetSizeAdjustment", ExploitativeBetSizeAdjustment),
        Napi::PropertyDescriptor::Function("exploitativeCallThresholdAdjustment", ExploitativeCallThresholdAdjustment),
        Napi::PropertyDescriptor::Function("villainLineRangeShift", VillainLineRangeShift),
        Napi::PropertyDescriptor::Function("villainCappedRangeScore", VillainCappedRangeScore),
        Napi::PropertyDescriptor::Function("villainPolarizedRangeScore", VillainPolarizedRangeScore),
        Napi::PropertyDescriptor::Function("villainFloatFrequencyEstimate", VillainFloatFrequencyEstimate),
        Napi::PropertyDescriptor::Function("legalActionSummary", LegalActionSummary),
        Napi::PropertyDescriptor::Function("actionMaskFromState", ActionMaskFromState),
        Napi::PropertyDescriptor::Function("normalizeBotConfig", NormalizeBotConfig),
        Napi::PropertyDescriptor::Function("validatePokerState", ValidatePokerState),
        Napi::PropertyDescriptor::Function("stateToFeatureVector", StateToFeatureVector),
        Napi::PropertyDescriptor::Function("actionEvBreakdown", ActionEvBreakdown),
        Napi::PropertyDescriptor::Function("decideActionWithDiagnostics", DecideActionWithDiagnostics),
        Napi::PropertyDescriptor::Function("explainDecisionFactors", ExplainDecisionFactors),
        Napi::PropertyDescriptor::Function("candidateActionSet", CandidateActionSet),
        Napi::PropertyDescriptor::Function("runBotPolicyBatch", RunBotPolicyBatch),
    });
    return exports;
}

NODE_API_MODULE(poker_calculations, Init)
