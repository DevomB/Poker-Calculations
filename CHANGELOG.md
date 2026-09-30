# Changelog

## 4.0.2

TypeScript definitions only; no runtime changes.

- **`index.d.ts` is valid TypeScript.** It combined `export =` with named exports (TS2309), so projects with `skipLibCheck: false` failed to compile. The types now live in a namespace merged with the export, and `import type { CardInput } from 'poker-calculations'` works as documented. CI compiles a consumer file against the definitions.
- **`icmHarvillePlacementProbabilities`** is typed as the flat row-major `number[] | Float64Array` it has always returned (it was declared `number[][]`).
- Removed 34 type declarations left over from exports deleted in 4.0.0.

## 4.0.1

Bug fixes found while writing runnable examples for every export.

- **Aborting an async call no longer crashes Node.** Calling `abort()` on the `AbortSignal` passed to any `*Async` export threw "Unknown failure" from inside the abort event and terminated the process once the work had started. The promise now rejects with `AbortError`.
- **State APIs accept PKST bytes.** `decideAction`, `decideActionAsync`, `legalActionSummary`, `actionMaskFromState`, `validatePokerState`, `stateToFeatureVector`, and `runBotPolicyBatch` rejected the `Uint8Array` from `encodePokerState` with "state.players must be an array".
- **`icmPairwiseBubbleFactor`** handles a pot that busts a player (the usual bubble spot) instead of throwing. Busted seats take the bottom prize, as in the bounty and future-game functions, which now share one implementation.
- **`sprAfterCall` and `stackToPotAfterCall`** use the package's pot convention: `potBeforeCall` already includes villain's bet, so the pot after the call is `potBeforeCall + toCall`. They previously added the call twice.
- **Windows:** `formatPotOdds` and `formatPotOddsReducedFraction` return `∞:1` instead of `?:1` when there is nothing to call (sources are now compiled as UTF-8 on MSVC).

## 4.0.0

Correctness release. Several results change, 69 exports are removed, and the package is now feature-complete: later releases are bug fixes only.

### Fixed — results change

- **Fast hand evaluator.** Pairs, trips, and quads were only detected when they were the highest rank in the five cards. Every Monte Carlo and exact equity path built on it returned wrong numbers in **2.2.0–3.2.0** (for example 55 on A♥K♦Q♣9♠8♥: true equity 0.398, 3.2.0 returned 0.154). The fast evaluator now agrees with the reference evaluator on random 5-, 6-, and 7-card hands.
- **Call EV convention.** `expectedValueCall` counted hero's own call as winnings. It is now `equity × pot − (1 − equity) × toCall`, with `pot` including villain's bet, so it is zero exactly at `breakevenCallEquity`. `decideAction` inherits the fix. See [NUMERICAL.md](NUMERICAL.md#pot-convention-chip-math).
- **Rake variants** reduce to their unraked base at zero rake.
- **Multi-street pure bluffs:** two- and three-street bluff EV agrees with its breakeven solvers.
- **PKO knockout matrix** pays out every bounty except the eventual winner's (columns sum to P(player busts)).
- **Tournament duel** is closed-form gambler's ruin and now uses both stacks.
- **Polarized river indifference bet** is the closed form instead of a bisection that could collapse.
- **Hand potential:** EHS2 uses Billings' `HS² (1 − NPot) + (1 − HS²) PPot`.
- **Range tools:** `materializeVillainRangeAfterBlockers` indexes the dense range correctly; `equityDeltaIfCardRemoved` treats the card as dead in the runout too; `preflopCombosFromNotationMinusBlockers` removes real combos.
- **Nash push/fold:** the heads-up solver uses ICM when payouts are supplied.
- **Final-table / FGS:** busted seats keep the prize they already won.
- **Omaha:** nuttedness excludes hero's own cards; wrap outs ignore made straights.
- **Stud / razz:** razz kickers no longer carry a phantom rank; exact stud equity enumerates the live pool when both hands are short.

### Fixed — crashes

- **Bad input no longer kills Node.** In 3.2.0, invalid arguments to about 170 exports (`spr(-1, 100)`, `icmExpectedPayouts([], [])`, `harringtonM(0, 0, 0)`, …) threw a C++ exception that aborted the whole process. Every native exception now surfaces as a normal JavaScript `Error` you can catch. A crash sweep that calls all 370 exports with edge-case inputs went from 719 process-killing calls to none.
- **Monte Carlo** (`simulateHandOutcome` and its async, parallel, detailed, and batch forms) rejects a hero hand that isn't 2 cards, a board over 5 cards, duplicate cards, and more villains than the deck can deal. A one-card hand used to read past the end of the hand and could segfault.
- **Typed-array inputs** are read from the view's own offset, so `subarray()` views of a larger buffer work. `materializeVillainRangeAfterBlockers` checks for a `Float64Array(1326)` before reading it, and sparse ranges reject non-`Int32Array` indices instead of reading past the buffer.
- `exactHuEquityVsRandomHand` with a five-card board no longer terminates the process.
- `exactHeroCategoryJointFlopToRiver` no longer writes past its table on flops where a royal flush can come.
- `exactMultiwayBestWorstRunout` validates input before checking the board size.

### Removed (69)

Exports that were broken, returned constants, were heuristic scores with no defensible model, or only aliased one of those:

`actionEvBreakdown`, `bayesianRangeUpdateFromAction`, `blockerAwareBluffFrequency`, `bluffCatchDecisionScore`, `boardEquityShiftDistribution`, `boardFlushPressure`, `boardNutAdvantageApprox`, `boardPairednessIndex`, `boardRangeInteractionScore`, `boardRiverScareCardScore`, `boardStaticnessIndex`, `boardStraightPressure`, `boardTextureScore`, `boardTurnVolatility`, `boardWetnessScore`, `candidateActionSet`, `cbetSizeEvGrid`, `cfrHeadsUpPushFoldSolve`, `checkRaiseSemiBluffEv`, `classifyBoardTexture`, `delayedCbetRunoutScore`, `duplicationAdjustedOuts`, `effectivePotOddsDisplayAfterRake`, `enumerateScareCards`, `equityDenialValue`, `equityRealizationPenalty`, `exactEquityDistributionVsRange`, `exactEquityPercentileVsRange`, `exactEquityRealizationEstimate`, `exactMultiwayAheadFrequency`, `explainDecisionFactors`, `exploitativeBetSizeAdjustment`, `exploitativeCallThresholdAdjustment`, `foldEquityNeededByStreetPlan`, `futureGameSimulationPayouts`, `futureGrowthShare`, `heroBoardConnectivityScore`, `icmCallVsFoldEv`, `icmFieldPressureIndex`, `icmJamVsFoldEv`, `icmPayJumpSurvivalEv`, `icmShapleyValues`, `icmStallingEv`, `impliedBreakevenTotalPot`, `isoRaiseVsLimpersEv`, `lateRegOverlayEv`, `multiStreetStackOffThreshold`, `multiwayEquityIndependenceGap`, `nashFirstInJamRange`, `nashMultiwayShoveCall`, `opponentShowdownBiasEstimate`, `overbetPolarizationScore`, `probeBetEvGrid`, `protectionBetBenefit`, `rangeBoardCoverage`, `reverseImpliedOddsMaxFutureLoss`, `riverBluffCandidateScore`, `riverCallThresholdDistribution`, `riverValueBetThreshold`, `showdownValueIndex`, `sidePotLayerTournamentEvDelta`, `solveStageMinimaxRegretBet`, `spinGoNashJamCall`, `turnBarrelRunoutEvDistribution`, `valueTargetingScore`, `villainCappedRangeScore`, `villainFloatFrequencyEstimate`, `villainLineRangeShift`, `winnerTakeAllSatelliteEv`.

### Added (39)

- **Omaha Hi-Lo (PLO-8):** `evaluateOmahaHiLo`, `evaluateOmahaLoHand`, `omahaLoQualifies`, `omahaLoNutsOnBoard`, `exactHuOmahaHiLoEquity`, `simulateOmahaHiLoEquity`, `omahaHiLoMultiwayMc`, `omahaScoopProbabilityMc`, `omahaQuarterProbabilityMc`, `omahaHiLoNuttedness`.
- **Big O (5-card Omaha):** `evaluateBigOBestHand`, `evaluateBigOHandStrength`, `exactHuBigOEquityVsKnown`, `simulateBigOEquityVsRandom`, `simulateBigOEquityVsRange`, `bigOMultiwayEquityMc`, `bigONutsOnBoard`, `bigOComboCount`.
- **2-7 single draw:** `evaluateDeuceSevenHand`, `evaluateDeuceSevenCategory`, `deuceSevenIsPat`, `deuceSevenNutsPat`, `deuceSevenDrawEquityVsKnown`, `deuceSevenRoughVsSmooth`, `deuceSevenMultiwayShowdown`.
- **Stud / razz:** `evaluateStudBestHand`, `evaluateRazzHand`, `razzWheelIsNuts`, `studDeadCardDeck`, `exactHuStudEquityVsKnown`, `exactHuRazzEquityVsKnown`, `simulateStudEquityVsRandom`, `simulateRazzEquityVsRandom`.
- **Flop EHS2 buckets:** `ehs2BucketsVsRange`, `bucketMassFromRange`, `flopBucketStrategyTo1326`, `flopBucketCountDefault`, `canonicalFlopCfrKey`.
- **Exact multiway:** `exactMultiwayWinTieLoseKnownHands` (win / split / lose for 2–9 known hands).

### Changed signatures

- `handPotentialBreakdown(heroHoleCards, boardCards, range, options?)` — new optional `{ streets?: 1 | 2 }`.
- `preflopCombosFromNotationMinusBlockers(notation, deadCards)` — takes the dead cards themselves instead of a precomputed count.

### Tooling

- `npm test` (Node's built-in runner) covers every fix above and runs in CI.
- `npm run sweep` calls every export under 17 edge-case scenarios in child processes and fails if any call crashes the process.
- `FEATURES_ADDED.md` is now [`API.md`](API.md).

## Earlier releases

See the [commit history](https://github.com/DevomB/Poker-Calculations/commits/main) for 3.2.0 and before.
