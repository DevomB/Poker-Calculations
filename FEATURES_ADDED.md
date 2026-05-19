# Shipped feature inventory — `poker-calculations`

Complete inventory of what the **npm package** ships: **98** native JavaScript functions, **7** TypeScript result/state types, card conventions, and C++ engine primitives that are not re-exported to Node.

**Authoritative sources:** [`index.d.ts`](index.d.ts) (types + JSDoc), [`README.md`](README.md) (overview tables), [`native/binding.cpp`](native/binding.cpp) (`exports.Set`), [`examples/native-binding-exports.mjs`](examples/native-binding-exports.mjs) (runtime export list).

**Package metadata (v1.3.2):** Node **18+**; entry `index.js` + `index.d.ts`; prebuilt N-API binaries under `prebuilds/` (glibc + musl on Linux). No separate `poker-math.js` layer — all math is C++ via N-API.

---

## JavaScript / N-API exports (98 functions)

Implemented in C++ and registered in [`native/binding.cpp`](native/binding.cpp). C++-only engine APIs (`GameEngine`, deck lifecycle, `BotConfig` file I/O) are under [Engine and integration](#engine-and-integration).

| Group | Export | Role |
| --- | --- | --- |
| **Hand resolution** | `evaluateBestHand(cards)` | Best five of **1–7** cards; returns `{ rank, kickers }` (`HandEvalResult`). |
| | `evaluateHandStrength(holeCards, board)` | Encoded strength: decimal string of `uint64` (rank in high bits + five kicker nibbles); comparable across spots. |
| | `evaluateHandStrengthFast(holeCards, board)` | Same encoding as `evaluateHandStrength`; uses in-house **forge** stack evaluator (`src/fast_evaluator.cpp`). |
| | `benchmarkEvaluatorThroughput(iterations?)` | `{ legacyEvalsPerSecond, fastEvalsPerSecond, implementation }` for legacy vs forge paths. |
| | `evaluateHandCategory(holeCards, board)` | Category label only (`highCard` … `royalFlush`); see [Hand rank labels](#hand-rank-labels). |
| | `validateCardString(card)` | `true` if `card` parses as one card (`Ah`, `10c`, …). |
| | `cardStringsHaveDuplicate(cards[])` | `true` if two entries parse to the same card; throws if any string is invalid. |
| | `compareBestHands(cardsA[], cardsB[])` | `-1` / `0` / `1` best-hand order; throws on overlap or invalid cards. |
| | `canonicalCardString(card)` | Canonical two-character card (`Th`, `Ac`, …); throws if invalid. |
| | `parseCompactCardList(text)` | Parse concatenated or whitespace-separated cards (`AhKh`, `10h Kd`); throws on duplicate or invalid token. |
| | `handRankCategoryOrder(category)` | Integer order `0..9` for `evaluateHandCategory` labels (`highCard` … `royalFlush`). |
| **Monte Carlo equity** | `simulateHandOutcome(holeCards, board, numSimulations, seed, villains?)` | Estimated equity vs one or more random villain hands (default `villains = 1`). |
| | `parallelHandSimulation(holeCards, board, numSimulations, baseSeed, villains, numThreads)` | Same with worker threads and distinct seeds per chunk. |
| | `exactHuEquityVsRandomHand(heroHoleCards, boardCards)` | Exact HU equity vs uniform random villain hand; board must have **3–5** cards. |
| **Strategy** | `decideAction(state, config, opponentModel?, heroSeat?)` | Rule-based action using MC equity (or strength fallback when sim count is 0), pot odds, call EV; see [decideAction contract](#decideaction-contract). |
| **Pot / chip EV** | `potOddsRatio(pot, toCall)` | `toCall / (pot + toCall)` when valid; else `0`. |
| | `expectedValueCall(equity, pot, toCall)` | Chip EV of calling once vs folding (0); no future streets. |
| | `expectedValueCallWithRake(equity, potBeforeCall, toCall, rakeFraction, rakeCap)` | Chip EV of call vs fold when the final HU pot pays rake (same rake model as breakeven-with-rake). |
| | `breakevenCallEquity(potBeforeCall, toCall)` | Same fraction as `potOddsRatio` for chip calls. |
| | `breakevenCallEquityFromPotOddsDisplayRatio(displayPotToCallRatio)` | `1/(1+R)` from `potOddsRatioDisplay` ratio `R`; `0` when `R` is `+∞`. |
| | `potOddsDisplayRatioFromBreakevenCallEquity(breakevenEquity)` | Inverse: `(1−e)/e`; `+∞` when `e=0`; `0` when `e=1`. |
| | `formatPotOddsReducedFraction(potBeforeCall, toCall)` | Reduced integer ratio string `pot : to_call` (e.g. `100`,`50`→`2:1`); `toCall=0`→`∞:1`. |
| | `equityToWinningOddsAgainst(equity)` | Book-style `(1−p)/p`; `+∞` at `p=0`. |
| | `winningOddsAgainstToEquity(oddsAgainst)` | `1/(1+o)` for `o≥0`; `0` when `o=+∞`. |
| | `rakeFromPot(potChips, rakeFraction, rakeCap)` | Rake `min(fraction×pot, cap)` for rake-adjusted helpers. |
| | `breakevenCallEquityWithRake(potBeforeCall, toCall, rakeFraction, rakeCap)` | Breakeven equity when the **final** pot (after call) pays rake under that model. |
| **Stacks & display** | `spr(potChips, effectiveStackChips)` | Stack-to-pot ratio. |
| | `effectiveStack(...stacks)` | Minimum stack; empty → `0`. |
| | `normalizedStackFractions(stacks[])` | Each stack divided by sum of stacks (chip shares; not Harville ICM). |
| | `stackInBigBlinds(stackChips, bigBlind)` | Stack size in big blinds. |
| | `potOddsRatioDisplay(potBeforeCall, toCall)` | Display ratio `pot : to_call` (e.g. `3.5` means 3.5:1). |
| | `formatPotOdds(potBeforeCall, toCall, decimals?)` | Human-readable `"x:1"` string. |
| | `harringtonM(stackChips, smallBlind, bigBlind, totalAntes)` | Harrington **M** = stack / (sb + bb + antes). |
| | `harringtonMEffective(stackChips, smallBlind, bigBlind, antePerActivePlayer, numActivePlayers)` | Effective M = stack / (sb + bb + antes from active players only). |
| | `harringtonMEffectiveActiveAntes(stackChips, smallBlind, bigBlind, antesFromActiveSeats[])` | Same denominator with explicit per-seat antes for active players only. |
| | `harringtonQ(heroStack, stacks[])` | Harrington **Q** = hero stack / mean(table stacks); all stacks positive. |
| | `orbitCostChips(smallBlind, bigBlind, antesFromSeats[])` | One orbit posted cost: sb + bb + sum of antes. |
| | `nlMinimumRaiseToTotal(currentMaxWager, lastRaiseIncrement, bigBlind)` | NL toy minimum **total** wager after a raise: current + `max(last increment, BB)`. |
| | `preflopCombosFromNotation(notation)` | NLHE combo count from shorthand (`AA`, `AKs`, `AKo`); throws on invalid notation. |
| | `preflopCombosFromNotationsList(notations[])` | Sum of combo counts over a list; empty → `0`. |
| **Heuristics** | `ruleOfFourEquity(outs)` | Out-count × 4% cap heuristic (turn+river). |
| | `ruleOfTwoEquity(outs)` | Out-count × 2% cap heuristic (one card). |
| | `estimatedOutsFromRuleOfTwo(equity, unseenCards)` | Uncapped inverse-style estimate `equity×unseen/2` clamped to `[0, unseen]` (not exact inverse of capped rule-of-two). |
| | `estimatedOutsFromRuleOfFour(equity, unseenCards)` | Same for two streets: `equity×unseen/4` clamped. |
| | `impliedBreakevenFutureWin(potBeforeCall, toCall, equity)` | Average extra future win needed for a neutral call; `+∞` if equity ≤ 0. |
| | `hypergeometricOneCardHitProbability(outs, unseenCards)` | One-card draw: `outs / unseenCards`. |
| | `runnerRunnerBackdoorFlushTwoCardProbability(suitCardsRemaining, unseenCards)` | `C(s,2)/C(u,2)` for two-card runner flush. |
| | `flopToRiverAtLeastOneHitProbability(outs, unseenAfterFlop)` | Two streets, single effective out count, no hit on both misses. |
| | `flopToRiverAtLeastOneHitUnionTwoCategories(unseenAfterFlop, outsA, outsB, sharedAb)` | Two categories with overlap; union cardinality in the two-draw formula. |
| | `flopToRiverAtLeastOneHitUnionThreeCategories(...)` | Three categories; inclusion–exclusion on union size, then same two-street formula. |
| | `flopToRiverAtLeastOneHitUnionFourCategories(...)` | Four categories; full inclusion–exclusion on card-count intersections, then same two-street formula. |
| | `flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop, outsPerCategory[])` | Sum **disjoint** categories, then same two-street hit formula (categories must not share outs). |
| | `runnerRunnerStraightDrawHitProbability(straightKind, deadAmongPatternOuts, unseenAfterFlop)` | Runner–runner straight draw: `straightKind` `0` gutshot (4) / `1` OESD / `2` double-belly (8), minus dead among pattern outs. |
| | `straightMadeFlopToRiverExactProbability(heroHoleCards[], flopThree[], knownDead[])` | Exact probability of straight or better in hero’s best 7 after uniform random unordered turn+river from remaining deck. |
| | `duplicationAdjustedOuts(outs, numVillains, duplicationWeight)` | Heuristic `outs / (1 + weight × villains)`. |
| **Reverse implied / geometry** | `reverseImpliedOddsMaxFutureLoss(potBeforeCall, toCall, equity)` | Toy ceiling on extra future loss when losing while keeping the call ≥ 0 EV. |
| | `geometricPotAfterMatchedPotFractions(pot0, fraction, nRounds)` | Pot after `nRounds` of matched pot-fraction HU betting: `pot0 × (1 + 2f)^n`. |
| **Stats & risk** | `monteCarloStandardError(pHat, nTrials)` | Binomial SE `√(p̂(1−p̂)/n)`. |
| | `monteCarloTrialsForStandardErrorBound(pHat, targetSe)` | Smallest integer `n` with SE ≤ `targetSe` at interior `pHat` (ceil of `p(1−p)/se²`). |
| | `wilsonScoreInterval(successes, nTrials, z)` | Wilson interval; returns `{ lower, upper }`. |
| | `agrestiCoullInterval(successes, nTrials, z)` | Agresti–Coull interval (same `{ lower, upper }` shape). |
| | `normalWaldBinomialInterval(successes, nTrials, z)` | Normal (Wald) `p̂ ± z·SE` clamped to `[0,1]` (weak near 0/1 with small `n`). |
| | `monteCarloTrialsForHoeffdingBound(epsilon, delta)` | Hoeffding: smallest `n` with `n ≥ ln(2/δ)/(2ε²)` for uniform MC error bound. |
| | `riskOfRuinDiffusionApprox(driftPerHand, variancePerHand, bankroll)` | `exp(−2μB/σ²)` style ROR; returns `1` if drift ≤ 0. |
| | `bankrollForTargetRorDiffusion(driftPerHand, variancePerHand, targetRor)` | Inverse of `riskOfRuinDiffusionApprox` for bankroll `B`. |
| | `betaBinomialFoldPosterior(priorAlpha, priorBeta, folds, calls)` | Conjugate Beta update; returns `{ alpha, beta, posteriorMean }`. |
| **Kelly & jam toys** | `kellyCriterionBinary(winProbability, netOdds)` | Full Kelly `(p·b − (1−p)) / b` for net odds `b`. |
| | `chubukovSymmetricJamBreakevenStack(deadMoneyChips, equity)` | Toy symmetric jam: `S = equity·dead/(1−2·equity)` for `equity < 0.5`; `+∞` if `equity > 0.5`. |
| | `chubukovSymmetricJamEv(jamStackChips, deadMoneyChips, equity)` | Symmetric jam toy EV in chips. |
| | `chubukovMaxSymmetricJamStackChipsBinarySearch(equity, deadMoneyChips, maxStackChips)` | Largest integer jam stack in `[1, max]` with nonnegative EV for fixed equity. |
| | `chubukovMaxSymmetricJamStackBinarySearch(heroHoleCards[], boardCards[], deadMoneyChips, maxStackChips)` | Same search on equity from `exactHuEquityVsRandomHand` (board 3–5); `maxStackChips` clamped like native `double` → int cap. |
| | `chubukovMaxSymmetricJamStackFromHandBinarySearch(heroHoleCards[], boardCards[], deadMoneyChips, maxStackChips)` | Same integer search; `maxStackChips` read as **int32** in the binding (pair with the other export for large caps). |
| **GTO-style (toy)** | `minimumDefenseFrequency(potBeforeOpponentBet, opponentBetSize)` | MDF from pot geometry. |
| | `alphaFrequency(potBeforeBet, betSize)` | `1 - MDF` = exploit weight if hero never defends. |
| | `bluffToValueRatio(potBeforeBet, betSize)` | Polarized river combo ratio `bet / (pot + 2×bet)`. |
| | `valueToBluffRatio(potBeforeBet, betSize)` | Reciprocal; `Infinity` when bet is 0. |
| **Sizing & commitment** | `betAsPotFraction(potBeforeBet, betSize)` | Bet as fraction of pot. |
| | `sprAfterCall(potBeforeCall, toCall, effectiveStackBeforeCall)` | SPR after HU single call; throws if `toCall` > stack. |
| | `commitmentRatio(toCall, effectiveStackBeforeCall)` | Fraction of stack put in to call. |
| **Fold equity** | `breakevenFoldEquityPureBluff(potBeforeHeroBet, heroBetOrCallSize)` | FE when equity if called is 0. |
| | `breakevenFoldEquitySemiBluff(potBeforeHeroBet, heroBetSize, equityWhenCalled, totalPotIfCalled)` | Two-outcome model; may exceed 1 if line is −EV even if villain always folds. |
| | `breakevenFoldEquitySemiBluffWithRake(..., rakeFraction, rakeCap)` | Semi-bluff FE with rake on `totalPotIfCalled`. |
| | `breakevenFoldEquityPureBluffWithRake(...)` | Pure-bluff FE parallel to semi-bluff rake model. |
| | `twoStreetPureBluffSameFoldEquity(potBeforeStreet1, betStreet1, betStreet2)` | Same FE both streets, pure air; may return `NaN`. |
| | `twoStreetPureBluffEv(..., foldEquityStreet1, foldEquityStreet2)` | Two-street pure-bluff chip EV with independent FE per street. |
| | `breakevenFoldEquitySecondStreetPureBluff(..., foldEquityStreet1)` | Breakeven second-street FE given first-street FE. |
| | `breakevenFoldEquityFirstStreetPureBluff(..., foldEquityStreet2)` | Breakeven first-street FE given second-street FE. |
| **Multiway** | `multiwaySymmetricBreakevenCallEquity(potBefore, toCall, symmetricExtraCallers)` | `k` extra symmetric callers. |
| | `multiwaySymmetricBreakevenCallEquityWithShare(..., shareModel, heroFractionWhenWin)` | Same geometry; `shareModel` `0` winner-take-all, `1` hero gets `heroFractionWhenWin` of final pot when winning. |
| **ICM** | `icmWinProbabilitiesHarville(stacks[])` | Harville first-place probabilities. |
| | `icmHarvillePlacementProbabilities(stacks[])` | Full `n×n` Harville placement matrix (per player, per finish rank). |
| | `icmTopKFinishProbabilities(stacks[], k)` | Sum of Harville placement over first `k` finish ranks per player (convenience on placement matrix). |
| | `icmLastPlaceProbabilitiesHarville(stacks[])` | Harville probability each player finishes **last** (placement matrix last column). |
| | `icmExpectedPayouts(stacks[], payouts[])` | Expected payout per seat. |
| | `icmPairwiseBubbleFactor(stacks[], payouts[], heroIndex, villainIndex, potChips)` | Loss/gain ratio from finite differences on `icmExpectedPayouts`. |
| **Side pots** | `sidePotLadderFromCommitments(committedChips[])` | Main + side layers; each layer `{ potChips, playerCapContribution[] }`. |
| | `layeredPotChipEvFromEquities(layerPotChips[], equityPlayerByLayer[][])` | Chip EV; each column sums to `1`. |
| | `sidePotLayersTotalChips(layers[])` | Sum of `potChips` across layers from `sidePotLadderFromCommitments`. |

### Alphabetical export index (98)

`agrestiCoullInterval`, `alphaFrequency`, `bankrollForTargetRorDiffusion`, `benchmarkEvaluatorThroughput`, `betAsPotFraction`, `betaBinomialFoldPosterior`, `bluffToValueRatio`, `breakevenCallEquity`, `breakevenCallEquityFromPotOddsDisplayRatio`, `breakevenCallEquityWithRake`, `breakevenFoldEquityFirstStreetPureBluff`, `breakevenFoldEquityPureBluff`, `breakevenFoldEquityPureBluffWithRake`, `breakevenFoldEquitySecondStreetPureBluff`, `breakevenFoldEquitySemiBluff`, `breakevenFoldEquitySemiBluffWithRake`, `canonicalCardString`, `cardStringsHaveDuplicate`, `chubukovMaxSymmetricJamStackBinarySearch`, `chubukovMaxSymmetricJamStackChipsBinarySearch`, `chubukovMaxSymmetricJamStackFromHandBinarySearch`, `chubukovSymmetricJamBreakevenStack`, `chubukovSymmetricJamEv`, `commitmentRatio`, `compareBestHands`, `decideAction`, `duplicationAdjustedOuts`, `effectiveStack`, `equityToWinningOddsAgainst`, `estimatedOutsFromRuleOfFour`, `estimatedOutsFromRuleOfTwo`, `evaluateBestHand`, `evaluateHandCategory`, `evaluateHandStrength`, `evaluateHandStrengthFast`, `exactHuEquityVsRandomHand`, `expectedValueCall`, `expectedValueCallWithRake`, `flopToRiverAtLeastOneHitDisjointOutsSum`, `flopToRiverAtLeastOneHitProbability`, `flopToRiverAtLeastOneHitUnionFourCategories`, `flopToRiverAtLeastOneHitUnionThreeCategories`, `flopToRiverAtLeastOneHitUnionTwoCategories`, `formatPotOdds`, `formatPotOddsReducedFraction`, `geometricPotAfterMatchedPotFractions`, `handRankCategoryOrder`, `harringtonM`, `harringtonMEffective`, `harringtonMEffectiveActiveAntes`, `harringtonQ`, `hypergeometricOneCardHitProbability`, `icmExpectedPayouts`, `icmHarvillePlacementProbabilities`, `icmLastPlaceProbabilitiesHarville`, `icmPairwiseBubbleFactor`, `icmTopKFinishProbabilities`, `icmWinProbabilitiesHarville`, `impliedBreakevenFutureWin`, `kellyCriterionBinary`, `layeredPotChipEvFromEquities`, `minimumDefenseFrequency`, `monteCarloStandardError`, `monteCarloTrialsForHoeffdingBound`, `monteCarloTrialsForStandardErrorBound`, `multiwaySymmetricBreakevenCallEquity`, `multiwaySymmetricBreakevenCallEquityWithShare`, `nlMinimumRaiseToTotal`, `normalWaldBinomialInterval`, `normalizedStackFractions`, `orbitCostChips`, `parallelHandSimulation`, `parseCompactCardList`, `potOddsDisplayRatioFromBreakevenCallEquity`, `potOddsRatio`, `potOddsRatioDisplay`, `preflopCombosFromNotation`, `preflopCombosFromNotationsList`, `rakeFromPot`, `reverseImpliedOddsMaxFutureLoss`, `riskOfRuinDiffusionApprox`, `ruleOfFourEquity`, `ruleOfTwoEquity`, `runnerRunnerBackdoorFlushTwoCardProbability`, `runnerRunnerStraightDrawHitProbability`, `sidePotLadderFromCommitments`, `sidePotLayersTotalChips`, `simulateHandOutcome`, `spr`, `sprAfterCall`, `stackInBigBlinds`, `straightMadeFlopToRiverExactProbability`, `twoStreetPureBluffEv`, `twoStreetPureBluffSameFoldEquity`, `validateCardString`, `valueToBluffRatio`, `wilsonScoreInterval`, `winningOddsAgainstToEquity`.

## Card strings

| Rule | Detail |
| --- | --- |
| Ranks | `2`–`9`, `T`, `J`, `Q`, `K`, `A`, or `10` |
| Suits | `c`, `d`, `h`, `s` (case-insensitive rank/suit in parser) |
| Canonical form | Two characters after parse (`Th`, `Ac`); tens use `T` in canonical output |
| Lists | Space or concatenation (`AhKh`, `Ah Kh`, `10hKd`); duplicates throw |
| Compare / MC / exact | Known cards must not overlap; invalid strings throw `Error` with message |

---

## Hand rank labels

Returned by `evaluateBestHand` → `rank` and `evaluateHandCategory`. `handRankCategoryOrder` maps name → `0..9`.

| Order | Label |
| ---: | --- |
| 0 | `highCard` |
| 1 | `onePair` |
| 2 | `twoPair` |
| 3 | `threeOfAKind` |
| 4 | `straight` |
| 5 | `flush` |
| 6 | `fullHouse` |
| 7 | `fourOfAKind` |
| 8 | `straightFlush` |
| 9 | `royalFlush` |

`evaluateBestHand` → `kickers`: length-5 array of encoded kicker values (internal rank ordering for tie-breaks).

---

## TypeScript types (inputs / outputs)

| Type | Fields / values |
| --- | --- |
| **`HandEvalResult`** | `rank: string`, `kickers: number[]` (length 5) |
| **`DecisionResult`** | `action: 'fold' \| 'call' \| 'raise' \| 'check'`, `raiseBy: number` (chips above call for raises) |
| **`WilsonScoreInterval`** | `lower`, `upper` (same shape for Agresti–Coull and Wald intervals) |
| **`BetaBinomialFoldPosterior`** | `alpha`, `beta`, `posteriorMean` |
| **`SidePotLayer`** | `potChips`, `playerCapContribution: number[]` |
| **`NativePokerState`** | See [decideAction contract](#decideaction-contract) |
| **`NativeBotConfig`** | `aggressionThreshold?`, `riskTolerance?`, `monteCarloSimulations?`, `monteCarloVillains?`, `raisePotFraction?`, `opponentAggressionWeight?`, `rngSeed?` |
| **`NativeOpponentModel`** | `aggressionFactor?`, `callFrequency?`, `foldFrequency?` |

C++ defaults for `BotConfig` (when fields omitted): aggression `0.55`, risk `0.92`, MC sims `800`, villains `1`, raise pot fraction `0.55`, opponent aggression weight `0.05`, rng seed `2463534242`.

---

## decideAction contract

Serialized table state (camelCase JSON-shaped object) plus bot config; optional opponent model and hero seat.

**`NativePokerState` (required / common fields)**

| Field | Required | Notes |
| --- | --- | --- |
| `players[]` | yes | Each: `holeCards: string[]` (required), optional `name`, `stack`, `committedThisStreet`, `totalCommittedHand`, `folded`, `seat` |
| `communityCards` | yes | Board card strings |
| `phase` | yes | See phase strings below |
| `actedThisStreet` | yes | Boolean array, one per player |
| `pot`, `currentBet`, `buttonSeat`, `smallBlind`, `bigBlind`, `actingIndex`, `lastRaiseIncrement`, `streetOpeningIndex` | optional | Numeric; sensible defaults in parser |

**Phase strings (accepted):** `PreFlop` / `preflop`, `Flop` / `flop`, `Turn` / `turn`, `River` / `river`, `Showdown` / `showdown`, `HandComplete` / `handcomplete`.

**Hero resolution:** `heroSeat` if passed; else acting player’s seat; else first player’s seat.

**Strategy behavior:** Uses `monteCarloSimulations` / `monteCarloVillains` from config when &gt; 0; otherwise falls back to encoded hand strength. Returns `DecisionResult`.

---

## Runnable examples (`examples/`)

| Script | FEATURES_ADDED groups covered |
| --- | --- |
| `native-binding-exports.mjs` | All 96 exports (runtime list) |
| `hand-resolution.mjs` | Hand resolution |
| `monte-carlo-equity.mjs` | Monte Carlo + exact equity |
| `strategy-decide-action.mjs` | `decideAction` |
| `pot-chip-ev-rake.mjs` | Pot / chip EV, rake |
| `stacks-display.mjs` | Stacks & display |
| `heuristics-draws.mjs` | Heuristics & draws |
| `reverse-implied-geometry.mjs` | Reverse implied / geometry |
| `stats-risk.mjs` | Stats & risk |
| `kelly-chubukov-jam.mjs` | Kelly & Chubukov |
| `gto-frequency.mjs` | GTO-style frequencies |
| `sizing-commitment.mjs` | Sizing & commitment |
| `fold-equity.mjs` | Fold equity |
| `multiway.mjs` | Multiway |
| `icm.mjs` | ICM |
| `side-pots.mjs` | Side pots |
| `intervals-and-odds-bridge.mjs` | Intervals, pot-odds display ↔ breakeven |

---

## C++ modules

| Module | Headers | Role |
| --- | --- | --- |
| Core chip / odds / probability | [`include/poker/poker_math.hpp`](include/poker/poker_math.hpp) | Pot odds, MDF, fold FE, draw heuristics, multiway/fold-FE/rake/stats helpers, symmetric-jam toys, NL orbit / Q / min-raise / preflop combo toys, inverse rule-of-2/4 outs, MC trial planner (SE + Hoeffding), Wilson / Agresti–Coull / Wald intervals, pot-odds display ↔ breakeven equity, equity ↔ winning odds-against, normalized stack shares, `hand_rank_category_order`. |
| Card strings | [`include/poker/card_string.hpp`](include/poker/card_string.hpp) | Shared parse + duplicate detection; `canonical_card_string`, `parse_compact_card_list`. |
| Hand evaluation | [`include/poker/hand_evaluator.hpp`](include/poker/hand_evaluator.hpp) | Best hand, category, strength encoding, `compare_best_hands`. |
| Monte Carlo | [`include/poker/monte_carlo.hpp`](include/poker/monte_carlo.hpp) | `simulate_hand_outcome`, `parallel_hand_simulation`. |
| Exact equity | [`include/poker/exact_equity.hpp`](include/poker/exact_equity.hpp) | Enumeration equity vs random hand; exact flop→river straight-or-better; Chubukov max integer jam stack from hand. |
| Strategy | [`include/poker/strategy.hpp`](include/poker/strategy.hpp) | `decide_action` with `BotConfig`, optional `OpponentModel*`. |
| ICM | [`include/poker/icm.hpp`](include/poker/icm.hpp) | Harville full placement matrix, win probs, top‑k finish sums, last-place probabilities, $EV, bubble factor. |
| Side pots | [`include/poker/side_pot.hpp`](include/poker/side_pot.hpp) | Side-pot ladder, layered EV, `side_pot_layers_total_chips`. |
| Engine | [`include/poker/game_engine.hpp`](include/poker/game_engine.hpp), [`game_state.hpp`](include/poker/game_state.hpp), [`deck.hpp`](include/poker/deck.hpp) | Full hand lifecycle (not exported to Node). |
| Bot integration | [`include/poker/bot_config.hpp`](include/poker/bot_config.hpp), [`opponent_model.hpp`](include/poker/opponent_model.hpp), [`poker_bot_interface.hpp`](include/poker/poker_bot_interface.hpp) | Config file I/O, opponent model, bot interface hook. |

---

## Engine and integration

Not separate Node exports; available when linking **`poker_lib`** in C++ or via internal use from `decideAction` / simulators.

| Area | Included |
| --- | --- |
| **Cards / deck** | 52-card deck, shuffle with injected `std::mt19937`, deal, burn on board deals in `GameEngine`. |
| **State & rules** | `PokerGameState`, blinds, pot, per-street commits, phase machine (pre-flop → river → showdown), `GameEngine::apply_action` with `Decision`. |
| **Evaluation** | Best five of up to seven cards, full ranking + kickers, `evaluate_hand_strength` scalar encoding. |
| **Strategy** | `decide_action` with `BotConfig`, optional `OpponentModel*`. |
| **Simulation** | `simulate_hand_outcome`, `parallel_hand_simulation` (chunked workers, distinct seeds). |
| **Config** | `BotConfig::load_from_config_file` / `save_to_config_file` (`key=value`, `#` comments). |
| **Tests** | GoogleTest suite (deck, engine, evaluator, card strings, poker math, ICM, side pots, exact equity, strategy, opponent model, MC, config). |
| **C++ sketch** | `GameEngine::start_new_hand`, `apply_action`, `advance_phase_if_ready`; subclass `PokerBotInterface` or `MockPokerBotInterface` for integration tests. |

---

*Last verified: **96** exports in `binding.cpp` / `index.d.ts`, all listed above. Re-run `node examples/native-binding-exports.mjs` after adding bindings.*
