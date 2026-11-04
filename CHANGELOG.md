# Changelog

## 4.1.1

Bug fixes. Some intervals change, and inputs that used to give meaningless results now throw.

### Fixed — results change

- **Wilson intervals.** The interval divided its `z²/4n²` term by an extra `n`, so it was too narrow and could exclude its own estimate: 100 successes in 100 gave [0.97965, 0.98335] instead of [0.96301, 1]. This affects `wilsonScoreInterval`, `binomialProportionCiWidth`, and the `ciLow`/`ciHigh` of `simulateHandOutcomeDetailed` and `simulateEquityVsRanges`; a guaranteed win now reports an upper bound of 1.
- **No raises against opponents who are all in.** `legalActionSummary`, `actionMaskFromState`, the `decideAction` family, and the C++ `GameEngine` allowed hero to raise when every other player still in the hand had no chips left; hero now calls (or checks) or folds.
- **Card strings are parsed by one strict rule.** The native parser rejected lowercase ranks (`'ah'`, `'td'`) while `poker-calculations/encode` accepted them, and `encode` read `'Ahjunk'` as `Ah`. Both now accept rank and suit in either case, `'10'` for ten, and surrounding whitespace, and reject anything else.

### Now rejected

- `benchmarkEvaluatorThroughput` / `benchmarkEvaluatorThroughputAsync` with an iteration count that is not an integer from 1 to 100,000,000; NaN or Infinity used to run for hours.
- Dead cards that are already in hero's hand or on the flop in `straightMadeFlopToRiverExactProbability`, `flushMadeFlopToRiverExactProbability`, `fullHouseMadeFlopToRiverExactProbability`, `tripsMadeFlopToRiverExactProbability`, and `twoPairMadeFlopToRiverExactProbability` (and their async forms); hero cards that are also on the board in `materializeVillainRangeAfterBlockers` and `exactRangeDominatedComboFraction`.
- Packed card input that is not a `Uint8Array` (or `Buffer`). A `Float64Array`, `Int32Array`, `Uint16Array`, or `DataView` had its raw bytes read as deck ids.
- An invalid `knownDead` in `exactVillainLeapfrogOutCounts` and `exactHeroCategoryJointFlopToRiver`, which was silently treated as no dead cards.

## 4.1.0

Correctness release for the decision, state, and Monte Carlo uncertainty APIs. Some results change, and inputs that used to produce meaningless results are now rejected.

### Fixed — results change

- **`simulateHandOutcomeDetailed` uncertainty.** `se` applied the Bernoulli formula to split-pot payoffs and ignored the sample variance it already tracked: when every player plays the board it reported 0.05 for payoffs that are always exactly 0.5. `se` is now the standard error from the sampled payoff variance (0.5 for a single trial), and `ciLow`/`ciHigh` is a Wilson interval at the unrounded mean instead of at a rounded success count. See [NUMERICAL.md](NUMERICAL.md#monte-carlo-uncertainty). `monteCarloStandardError` is unchanged.
- **Short all-in calls in `decideAction`.** A call for less than the bet was priced as if hero could win the whole pot for the whole bet. It now costs hero's stack and wins only what hero matches (side-pot layers): 10 behind against a 100 bet into 10 at 40% equity is worth +2, not −16. This applies to `decideAction`, `decideActionAsync`, `decideActionWithDiagnostics`, and `runBotPolicyBatch`.
- **Raise sizes stay legal.** `raiseBy` is at least the minimum raise and at most all-in; a large `raisePotFraction` used to return raises bigger than the stack.
- **Short all-in raises do not reopen betting.** `legalActionSummary` and `actionMaskFromState` no longer offer a re-raise to a player who already acted and faces less than a full raise, `minRaiseTo` is all-in when the stack is shorter than a full raise, and a finished hand or a folded or all-in player has no legal actions.
- **`decideActionWithDiagnostics`** reports hero's legal actions rather than the acting player's. **`runBotPolicyBatch`** applies `opponentModels[i]` to state `i` (they were ignored) and reports an unplayable state in that row instead of failing the batch.
- **`rangeTopCombos` and `rangeBucketWeightsByNotation`** list equal weights in combo order on every platform (the order of ties depended on the C++ standard library).
- **Seeds.** `rngSeed` reduces modulo 2^32 (a non-finite value keeps the default) and the per-decision seed no longer relies on signed overflow; ordinary seeds give the same results.
- **`GameEngine` (C++ only).** Heads-up the button posts the small blind and acts first before the flop; a short big blind still leaves the full big blind to call; a short all-in raise keeps the last full raise and does not reopen betting; the hand ends when one player is left; all-in hands run out to showdown instead of stalling.

### Added

- **`simulateEquityVsRanges(heroHoleCards, boardCards, ranges, numSimulations, seed, options?)`**: Monte Carlo showdown equity against one weighted range per opponent (1–9), with optional dead cards. Opponent hands are drawn jointly, rejecting whole tuples that share a card (bounded by `maxAttempts`), so the estimate follows the joint range distribution rather than a sequential approximation. It returns the uncertainty fields of `simulateHandOutcomeDetailed` plus the number of draws. See [NUMERICAL.md](NUMERICAL.md#multiway-range-equity).

### Now rejected

- **`decideAction` with `monteCarloSimulations` below 1.** The zero-simulation fallback read an encoded hand rank as an equity and could fold a royal flush. `normalizeBotConfig` now clamps the count to at least 1.
- **`decideAction` when hero cannot act:** an empty `heroSeat`, a folded or all-in hero, or a finished hand. Hero's missing hole cards are no longer replaced by player 0's.
- **Inconsistent states.** Every state API (`validatePokerState`, the `decideAction` family, `legalActionSummary`, `actionMaskFromState`, `stateToFeatureVector`, `runBotPolicyBatch`, `encodePokerState`, `decodePokerState`) applies one rule set to objects and PKST bytes: integer chip fields that fit in 32 bits, checked before conversion instead of rounded or wrapped; non-negative stacks, pot, and commitments; unique seats and cards; a board length that matches the phase; one boolean `actedThisStreet` flag per player; and actor indices in range. `validatePokerState` lists every problem. Unknown opponent hands (`holeCards: []`) and finished hands are still valid snapshots; PKST cannot encode an unknown hand.
- `simulateHandOutcomeDetailed` with `numSimulations` below 1 (it returned a zero-width interval at 0).
- Duplicate cards in Hold'em evaluation (`evaluateBestHand`, `evaluateHandStrength`, `evaluateHandStrengthFast`, `evaluateHandCategory`, `evaluateHandStrengthFastBatch`) and exact equity (`exactHuEquityVsKnownHand`, `exactHuEquityVsRandomHand`, `exactHuEquityVsRange`, the batch form, and the exact runout and category functions). They returned numbers for impossible hands, while the Monte Carlo and variant APIs already threw.
- Fractional counts in `betaBinomialFoldPosterior` / `opponentFoldToCbetPosterior` (they were truncated), negative counts in `opponentAggressionFactor`, and `opponentRangeElasticityFromSizing` arrays of different lengths.

### Documentation and tooling

- `index.d.ts` states which operations are Hold'em-only, the state and decision contracts, and what each opponent-modeling helper measures. `NativeOpponentModel.aggressionFactor` is a normalized score, unlike the `(bets + raises) / calls` ratio of `opponentAggressionFactor`.
- `npm test` also runs the 17 `scripts/verify-*.mjs` checks, with bounded workloads; their tolerance checks no longer let NaN through. `npm run test:native` builds C++ checks for `GameEngine` and the combo and hand-class index helpers.
- Publishing requires the unit, verify, type, C++, and crash-sweep checks and every platform binary, installs the packed tarball to smoke it, and tags the published commit `v<version>`.
- The package no longer ships the 1.58 MB README image or `prebuilds/README.md`. `API.md` is removed: `index.d.ts` and the documentation site cover it.

## 4.0.3

- **Hands under five cards are categorized.** `evaluateBestHand`, `evaluateHandCategory`, `evaluateHandStrength(Fast)`, `compareBestHands`, and `evaluateShortDeckBestHand` reported every 1–4 card hand as high card, so a pocket pair or a stud upcard pair ranked below an unpaired hand. They now report one pair, two pair, three of a kind, and four of a kind (straights, flushes, and full houses need five cards). Hands of five or more cards are unchanged.
- The crash sweep (`npm run sweep`) found no exports after the 4.0.2 namespace change and silently passed; it reads the new layout again.

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

Correctness release. Several results change and 69 exports are removed. Upgrade from any of 2.2.0–3.2.0: the fast hand evaluator in those releases skewed every Monte Carlo and exact equity result (first item below).

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
- `FEATURES_ADDED.md` was renamed `API.md`.

## Earlier releases

See the [commit history](https://github.com/DevomB/Poker-Calculations/commits/main) for 3.2.0 and before.
