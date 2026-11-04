# Changelog

## 4.1.3

Fixes for helpers that look at a flop or turn before the board is complete.

### Fixed — results change

- **Partial boards no longer include phantom cards.** Some helpers placed five or six real cards in a seven-card array and evaluated all seven slots, so the empty slots counted as two of clubs. With hero `Ah Kh` on `2h 7h 9c`, `exactHeroCategoryJointFlopToRiver` reported a turn high card 0% of the time (correct: 51.0638%), two pair 12.1184% and trips 12.4884% (correct: 0% each), and `exactVillainLeapfrogOutCounts` listed 9 hero-improving turn cards instead of 47. Hero and villain hands are now scored on exactly the cards dealt. Full-board results (`exactHeroRunoutVulnerability`, `exactRangeDominatedComboFraction`, runout quantiles) are unchanged.
- **`exactHeroCategoryJointFlopToRiver` counts both dealing orders.** It enumerated unordered turn-river pairs and always treated the lower deck index as the turn. With only `3h` and `As` live, it reported a turn flush 100% of the time instead of 50% (3h first) and a turn pair 50% (As first). Each pair now counts once in each order.

### Now rejected

- `exactHeroEquityRunoutQuantiles` and `exactHeroEquityRunoutQuantilesAsync` with any board other than a 3-card flop, with or without a range. Empty, one-card, and two-card boards returned made-up statistics (for `Ah Kd` and no board: a 94.12% mean, zero variance, `n: 1`) because the shared runout enumerator turned completions it does not support into "no cards to come"; the async form now rejects with the same message. The enumerator itself now throws for any board outside 3 to 5 cards.

### Tooling

- `test/partial-boards.test.js` checks the category matrix, both dealing orders, and the leapfrog lists against a brute-force reference that scores exactly the cards dealt, and the board-length checks for the synchronous and asynchronous exports.

## 4.1.2

Fixes a range-weight regression introduced in 4.1.1, a process crash in the side-pot helpers, and several chip-math and runout formulas, and closes gaps in range input handling and the TypeScript declarations.

### Fixed — results change

- **Range weights are scaled per calculation, from the combos that take part.** 4.1.1 rescaled a whole range by its largest weight before blockers and card conflicts were applied, so a positive, representable weight could become 0 once the dominant combo dropped out: `materializeVillainRangeAfterBlockers` turned `[1e200, 1e-200]` into `[1e200, 0]`, and `evOfStrategyProfile` with bettor `KhKd × 1e200, 3c4d × 1e-200` against defender `KhKd × 1e200, 5c6d × 1e-200` on `2c 7d 9s Jc Qh` returned EV 0 / 0 instead of 62.5 / 37.5. Ranges now keep the caller's weights; each calculation scales by the largest weight among its live combos, and the river CFR subgames accumulate pair weights (`w_bettor × w_defender`) and bet-weighted values as mantissa and exponent, so every conditional average is scaled by its own members. This covers exact and Monte Carlo Hold'em range equity, hand potential and EHS tables, bucket mass, the dominated-combo fraction, materialization, the CFR subgames, Omaha, Big O, and short-deck ranges, and the dense range tools. Ordinary ranges give bit-identical results to 4.1.1. See [NUMERICAL.md](NUMERICAL.md#range-weights).
- **A combo listed more than once in a sparse range is one combo with the summed weight**, in every export. `materializeVillainRangeAfterBlockers` used to report weights 2 and 3 for one combo as a dense weight of 3, a `weightSum` of 5, two live combos, and a nonzero entropy; it now reports 5, 5, one combo, and entropy 0, and `weightSum` is always the sum of `weights1326`.
- **`mergeSparseRanges`** forms each mixed weight without a shared scale, so a range is no longer zeroed by a much larger other range (for example with `weightA` 0), and huge mixing weights (`weightA = weightB = 1e308`) are normalized instead of rejected. **`pruneRangeByMinWeight`** normalizes the kept weights without overflowing (very large weights came back as 0).
- **`decideAction` prices calls into side pots layer by layer.** It applied one equity to the whole pot, so a call that wins a different share of each layer was mispriced: with a royal flush on board and commitments `[10, 1, 100]`, calling 90 collects 1 of the 3-chip main pot and 99 of the 198-chip side pot (+10), yet the bot folded with `monteCarloVillains: 2`. Hero's chips are now added to the commitments, `side_pot_ladder_from_commitments` splits the pot, and hero's share of each layer it can win is simulated against the players still in that layer (from one set of deals). Earlier streets count as one layer unless `totalCommittedHand` is consistent. When every layer has `monteCarloVillains` opponents (heads-up, for example) results are unchanged; multiway calls are now priced against the players actually in the pot rather than `monteCarloVillains`, which still sets the opponents when hero has nothing to call. Applies to `decideAction`, `decideActionAsync`, `decideActionWithDiagnostics`, and `runBotPolicyBatch`.
- **`breakevenFoldEquityPureBluffWithRake`** counted hero's returned bet as winnings: a fold wins the pot before the bet, so `(100, 50, 0, 0)` is 1/3, not 1/4. It now uses the payouts of `expectedValueRaiseWithRake` (fold: `pot − rake(pot + bet)`; called: lose the bet).
- **`breakevenFoldEquitySemiBluffWithRake`** ignored rake when villain folds. For pot 100, bet 50, called equity 10%, final pot 200, 5% rake capped at 1000 the threshold is 25.1012%, not 23.6641%; `expectedValueRaiseWithRake` at the returned threshold is now 0 (it was −1.7748).
- **`impliedBreakevenFutureWin`** subtracted the call twice: `(100, 50, 0.25)` needs 50 more chips, not 0. It is now `max(0, toCall / equity − pot − toCall)` and inverts `impliedOddsRequiredEquityFromFutureWin`.
- **Runout statistics against a range are predictive.** `exactHeroEquityRunoutQuantiles` with a range weighted every runout equally, although villain's range makes some boards likelier; each runout is now weighted by the range mass compatible with it, so the mean equals the direct exact equity (hero `Ah Ad`, flop `2c 7d 9s`, range `KhKd:1, 9c9d:9, 3c4d:1`: 24.0863%, where 4.1.1 reported 29.0590%). Runouts no combo can coexist with are skipped instead of throwing, so a single-combo range works. Quantiles interpolate the weighted distribution. Without a range (one random opponent) every runout is equally likely and results are unchanged.
- **`exactInformationRegretVsClairvoyant`** let only the clairvoyant fold: the baseline always called. Both sides now take `max(0, callEV)`, with runouts weighted as above, so the value is never negative and is 0 when no runout makes a call profitable.
- **Bluff-to-value ratios are ratios.** `bluffToValueRatio(pot, bet)` returned the bluff share of all bets, `bet / (pot + 2 bet)` (1/3 for a pot-sized bet), and `valueToBluffRatio` its inverse (3). They now return bluffs per value bet, `bet / (pot + bet)` (1/2), and value bets per bluff, `(pot + bet) / bet` (2); the rake variants use the raked pot the caller wins and return `Infinity` / 0 when the rake leaves the caller nothing to gain. The old bluff share is `breakevenCallEquity(pot + bet, bet)` (or `breakevenCallEquityWithRake`).
- **Card-removal sensitivity has no invented values.** `rangeRemovalSensitivityVsHero` caught every error and used equity 0, so removing a card that every combo holds reported `−base` (−1 for a lone `KhKd`), and invalid cards returned zeros. That entry is now `NaN` (the condition is impossible), as it is in `exactEquityCardRemovalGradient` (which reported 0), and other errors throw.
- **River CFR with a tiny bet frequency.** The bettor's weight was multiplied by its bet frequency before scaling, so a frequency near the smallest double could round the product to 0 and leave the defender's call value undefined (a valid spot called where folding was right). The frequency is now split into mantissa and exponent as well.
- **`simulateShortDeckEquityVsRange`** draws only from combos that hero and the board leave possible, instead of drawing blocked combos and discarding the trial; results for a seed change when the range contains blocked combos.

### Now rejected

- An equity matrix with a row of the wrong length in `layeredPotChipEvFromEquities` and `layeredPotChipEvFromEquitiesWithRake`. Only the first row was checked, so `[[0.5, 0.5], []]` read past the second row and crashed Node with an access violation.
- Sparse range `indices` entries that are not integer deck ids 0..51. `NaN`, `1.9`, and `4294967297` used to be converted to other, valid cards by integer conversion; every entry is now checked as given, for `number[]`, `Int32Array`, and `Uint32Array` alike. Packed Omaha and Big O combos get the same check, and a packed combo that repeats a card (silently skipped before) is rejected.
- `simulateEquityVsRanges` with `maxAttempts` below `numSimulations`. The budget was silently raised to `numSimulations`; each sample needs at least one draw, so such a budget cannot be honoured.
- River CFR subgames (`cfrRiverBetCallFoldSolve`, `fictitiousPlayRiver`, `evOfStrategyProfile`, `bestResponseRiver`, `exploitabilityRiver`, `solveHuRiverCheckBetTree`) in which every bettor combo shares a card with every defender combo; they returned EV 0.
- `rangeFromNotationWeights` entries whose weight is NaN, infinite, or negative.

### Fixed — no result change for valid input

- **One sparse range parser.** `exactRangeDominatedComboFraction`, `exactHeroEquityRunoutQuantiles`, `exactEquityCardRemovalGradient` (and their async forms), `exactInformationRegretVsClairvoyant`, and `materializeVillainRangeAfterBlockers` accepted only `Int32Array` indices; every export that takes a `SparseRangeSpec` now accepts `number[]`, `Int32Array`, and `Uint32Array` with the same validation and messages.
- **TypeScript.** `evaluateBestHand`, `evaluateOmahaBestHand`, `evaluateBigOBestHand`, `evaluateStudBestHand`, and `evaluateRazzHand` with `{ format: 'slim' }` are typed `HandEvalResultSlim` (the full-result overload matched first, promising `rank` and `kickers` that are not there); options only known at runtime give the union. `runBotPolicyBatch` returns `Array<DecisionDiagnosticResult | DecisionBatchErrorRow>`, so a row must be narrowed (`row.error !== undefined`) before its decision fields are read. New exported types: `EvaluateBestHandSlimOptions`, `EvaluateBestHandFullOptions`, `DecisionBatchErrorRow`.
- **C++ library.** `SparseRange` holds the caller's weights (`weight_sum` may be +infinity) and no longer has `weight_shift`; `rescale_range_weights` is replaced by `weight_scale_exponent`, `max_combo_weight`, and `ScaledWeightedSums` in `poker/range.hpp`.

### Tooling

- The scaling test in `test/range-consistency.test.js` now gives every field an exact expectation: scale-free fields (probabilities, frequencies, EVs, entropies, counts, indices) must be bit-identical at 2^100, 2^600, and 2^-600, and fields in weight units (listed per export) must scale by exactly that factor. New tests cover materialized totals against their arrays, a dominant combo that drops out, the CFR pair case above, duplicate combos, malformed indices, all three index formats in every native range parser, and the attempt budget; they fail on 4.1.1.
- `test/types/consumer.ts` checks the slim overloads and batch row narrowing; the installed-tarball smoke check covers the cases above.
- `test/review-4.1.2.test.js` checks each correction above against independently derived values: the side-pot call, the rake and implied-odds thresholds (by plugging them back into the EV functions), regret and runout statistics against a brute-force runout weighting, the bluff ratios, and the subnormal bet frequency.

## 4.1.1

Bug fixes. Some intervals change, and inputs that used to give meaningless results now throw.

### Fixed — results change

- **Wilson intervals.** The interval divided its `z²/4n²` term by an extra `n`, so it was too narrow and could exclude its own estimate: 100 successes in 100 gave [0.97965, 0.98335] instead of [0.96301, 1]. This affects `wilsonScoreInterval`, `binomialProportionCiWidth`, and the `ciLow`/`ciHigh` of `simulateHandOutcomeDetailed` and `simulateEquityVsRanges`; a guaranteed win now reports an upper bound of 1.
- **No raises against opponents who are all in.** `legalActionSummary`, `actionMaskFromState`, the `decideAction` family, and the C++ `GameEngine` allowed hero to raise when every other player still in the hand had no chips left; hero now calls (or checks) or folds.
- **Card strings are parsed by one strict rule.** The native parser rejected lowercase ranks (`'ah'`, `'td'`) while `poker-calculations/encode` accepted them, and `encode` read `'Ahjunk'` as `Ah`. Both now accept rank and suit in either case, `'10'` for ten, and surrounding whitespace, and reject anything else.
- **Range weights are scale-free.** A range's total weight overflowed or underflowed in every exact and Monte Carlo consumer: `[1e308, 1e308]` gave 0 or NaN where `[1, 1]` gave 0.5, Monte Carlo sampling picked combos from a broken distribution, and cosine similarity and the CFR subgames returned NaN or 0 at extreme scales. One shared helper now rescales extreme weights by an exact power of two before any total is formed, for Hold'em, Omaha, Big O, and short-deck ranges and the dense range tools. Ordinary ranges give bit-identical results, except `rangeCosineSimilarity`, which now normalizes first and can differ in the last digit. See [NUMERICAL.md](NUMERICAL.md#range-weights).

### Now rejected

- Range weights that are NaN, infinite, or negative (they were silently dropped, which changed the distribution), sparse range indices that are not a valid card pair, and weighted totals reported in the caller's units (`handPotentialBreakdown` counts, `materializeVillainRangeAfterBlockers`, class and per-card range totals) that exceed the largest double.
- `benchmarkEvaluatorThroughput` / `benchmarkEvaluatorThroughputAsync` with an iteration count that is not an integer from 1 to 100,000,000; NaN or Infinity used to run for hours.
- Dead cards that are already in hero's hand or on the flop in `straightMadeFlopToRiverExactProbability`, `flushMadeFlopToRiverExactProbability`, `fullHouseMadeFlopToRiverExactProbability`, `tripsMadeFlopToRiverExactProbability`, and `twoPairMadeFlopToRiverExactProbability` (and their async forms); hero cards that are also on the board in `materializeVillainRangeAfterBlockers` and `exactRangeDominatedComboFraction`.
- Packed card input that is not a `Uint8Array` (or `Buffer`). A `Float64Array`, `Int32Array`, `Uint16Array`, or `DataView` had its raw bytes read as deck ids.
- An invalid `knownDead` in `exactVillainLeapfrogOutCounts` and `exactHeroCategoryJointFlopToRiver`, which was silently treated as no dead cards.

### Tooling

- `test/range-consistency.test.js` checks every export that takes a weighted range: scaling all weights (field by field), sparse and dense input, opponent order, suit relabeling, an independent exact enumeration, and bounded rejection of impossible ranges. Its export list comes from `index.d.ts`, so a new range consumer must be covered.
- The installed-tarball smoke check (`scripts/smoke-load.js`, run on every platform build and on the packed package before publishing) exercises these fixes through the published entry points, including `poker-calculations/encode` resolved via the package's `exports` map.

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
