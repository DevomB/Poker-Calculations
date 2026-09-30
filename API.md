# API inventory — `poker-calculations`

Complete inventory of what the **npm package** ships: **370** native JavaScript functions, TypeScript result/state types, card conventions, and C++ engine primitives that are not re-exported to Node.

**Authoritative sources:** [`index.d.ts`](index.d.ts) (types + JSDoc), [`README.md`](README.md) (overview tables), [`native/binding_register.cpp`](native/binding_register.cpp) (export registration), [documentation site](https://poker-calculations.devomb.com/docs/reference/api) (examples + when-to-use), [`scripts/list-native-exports.mjs`](scripts/list-native-exports.mjs) (runtime export list).

**Documentation:** [poker-calculations.devomb.com](https://poker-calculations.devomb.com) — API reference, examples, and guides (replaces repo `examples/*.mjs` scripts).

**Package metadata:** Node **18+**; entry `index.js` + `index.d.ts`; optional [`encode.js`](encode.js) for packed cards and PKST state. Prebuilt N-API binaries under `prebuilds/` (glibc + musl on Linux). No separate `poker-math.js` layer — all math is C++ via N-API.

**Async vs C++ parallelism:** `parallelHandSimulation` uses C++ `std::async` inside the native call. The `*Async` exports (`simulateHandOutcomeAsync`, etc.) use Node’s libuv thread pool (`Napi::AsyncWorker`) so the **JavaScript event loop** stays responsive. Each `*Async` export accepts an optional trailing `{ signal?: AbortSignal }`; abort rejects with `AbortError` and stops CPU in cooperative hot loops (Monte Carlo, exact enumeration, benchmark, `decideAction` MC).

---

## JavaScript / N-API exports (370 functions)

Includes batch Monte Carlo, `*Async` Promise exports, Float64 ICM paths, and PKST packed state (`encodePokerState` / `decodePokerState`). ICM/stack helpers accept `Float64Array` and optional `returnFormat: 'float64'`. PKST byte layout: magic `PKST`, layout version byte, players, phase, pot fields, per-player hole bytes, community cards, `actedThisStreet` — see `include/poker/state_codec.hpp`.

Implemented in C++ and registered in [`native/binding_register.cpp`](native/binding_register.cpp). C++-only engine APIs (`GameEngine`, deck lifecycle, `BotConfig` file I/O) are under [Engine and integration](#engine-and-integration).

| Group | Export | Role |
| --- | --- | --- |
| **Hand resolution** | `evaluateBestHand(cards, options?)` | Best five of **1–7** cards; `HandEvalResult` (`rank`, `rankCategory`, `strength`, `kickers`) or `{ format: 'slim' }` → `rankCategory` + `strength` only. |
| | `evaluateHandStrength(holeCards, board)` | Encoded strength as `number` (`uint64` bit layout: rank in high bits + five kicker nibbles); `CardInput` for hole/board. |
| | `evaluateHandStrengthFast(holeCards, board)` | Same encoding as `evaluateHandStrength`; forge stack evaluator (`src/fast_evaluator.cpp`). |
| | `benchmarkEvaluatorThroughput(iterations?)` | `{ legacyEvalsPerSecond, fastEvalsPerSecond, implementation }` for legacy vs forge paths. |
| | `evaluateHandCategory(holeCards, board)` | Category label only (`highCard` … `royalFlush`); see [Hand rank labels](#hand-rank-labels). |
| | `validateCardString(card)` | `true` if `card` parses as one card (`Ah`, `10c`, …). |
| | `cardStringsHaveDuplicate(cards)` | `true` if two entries map to the same card; `CardInput` (`string[]` or packed `Uint8Array`). |
| | `compareBestHands(cardsA, cardsB)` | `-1` / `0` / `1` best-hand order; `CardInput`; throws on overlap or invalid cards. |
| | `canonicalCardString(card)` | Canonical two-character card (`Th`, `Ac`, …); throws if invalid. |
| | `parseCompactCardList(text, options?)` | Parse compact card text; default `string[]`, `{ outFormat: 'packed' }` → `Uint8Array` deck ids. |
| | `handRankCategoryOrder(category)` | Integer order `0..9` for `evaluateHandCategory` labels (`highCard` … `royalFlush`). |
| | `evaluateHandStrengthFastBatch(holes, boards, boardCards, out?)` | Packed batch of `evaluateHandStrengthFast` (see docs site batch guide). |
| | `buildPreflopEquityMatrix(options?)` | Row-major `169×169` preflop MC matrix (`PreflopMatrixOptions`). |
| | `exactHuEquityVsKnownHand(heroHoleCards, villainHoleCards, boardCards)` | Exact HU vs known villain; board empty or 3–5 cards. |
| | `exactHuEquityVsRange(heroHoleCards, boardCards, range)` | Exact HU vs dense `Float64Array(1326)` or sparse range spec. |
| **Exact multiway** | `exactThreeWayEquityKnownHands(hand0, hand1, hand2, board[, dead])` | Exact 3-way equity; known holes; board 0–5; equities sum to 1. |
| | `exactThreeWayWinTieLoseKnownHands(hand0, hand1, hand2, board[, dead])` | Per-player unique-win / split / lose frequencies. |
| | `exactMultiwayWinTieLoseKnownHands(holeHands, board[, dead])` | Exact win / split / lose per player for 2–9 known hands (general form of the 3-way export). |
| | `exactFourWayEquityKnownHands(hand0..hand3, board[, dead])` | Exact 4-way equity; known holes. |
| | `exactMultiwayEquityKnownHands(holeHands, board[, dead])` | Exact n-way (n = 3–6) known-hand equity; enumerates remaining boards. |
| | `exactMultiwayEquityWithDeadCards(holeHands, board, dead)` | Same with required dead/muck cards. |
| | `exactMultiwaySidePotChipEv(committed, holeHands, board[, dead])` | Side-pot chip EV from exact eligible-player equities per layer. |
| | `exactMultiwayTieFrequency(holeHands, board[, dead])` | P(hero split) and P(any split) at showdown. |
| | `exactMultiwayRunoutCount(holeHands, board[, dead])` | Remaining board count (river → 1). |
| | `exactMultiwayBestWorstRunout(holeHands, flopOrTurn[, dead])` | Best/worst next-street card for hero by exact equity; else `{ supported: false }`. |
| | `equityDeltaIfCardRemoved(heroHoleCards, boardCards, range, removedDeckIndex)` | Change in exact range equity when one deck id is dead. |
| **Monte Carlo equity** | `simulateHandOutcome(holeCards, board, numSimulations, seed, villains?)` | Estimated equity vs one or more random villain hands (default `villains = 1`). |
| | `simulateHandOutcomeAsync(…, options?)` | Same as sync; Promise on libuv thread pool; optional `AbortSignal`. |
| | `parallelHandSimulation(holeCards, board, numSimulations, baseSeed, villains, numThreads)` | Same with C++ worker threads and distinct seeds per chunk. |
| | `parallelHandSimulationAsync(…, options?)` | Same as sync; Promise on libuv thread pool; optional `AbortSignal`. |
| | `simulateHandOutcomeBatch(specs[])` | Many MC spots → `Float64Array` (`SimBatchSpec` per row). |
| | `simulateHandOutcomeBatchPacked(holes, boards, boardCards, meta, out?)` | Packed batch MC layout. |
| | `simulateHandOutcomeDetailed(holeCards, board, numSimulations, seed, villains?)` | `{ estimate, se, ciLow, ciHigh, n }` with Wilson CI. |
| | `simulateEquityVsRange(heroHoleCards, boardCards, range, numSimulations, seed)` | MC vs weighted villain range. |
| | `exactHuEquityVsRandomHand(heroHoleCards, boardCards)` | Exact HU equity vs uniform random villain hand; board **empty (preflop) or 3–5** cards. |
| | `exactHuEquityVsRandomHandBatch(holes, boards, boardCards, out?)` | Packed batch exact HU vs random. |
| | `exactHuEquityVsRandomHandAsync(…, options?)` | Same as sync; Promise on libuv thread pool; optional `AbortSignal`. |
| | `straightMadeFlopToRiverExactProbabilityAsync(…, options?)` | Async sibling of exact flop→river straight probability; optional `AbortSignal`. |
| | `benchmarkEvaluatorThroughputAsync(iterations?, options?)` | Async sibling of evaluator benchmark; optional `AbortSignal`. |
| **Strategy** | `decideAction(state, config, opponentModel?, heroSeat?)` | Rule-based action using MC equity (or strength fallback when sim count is 0), pot odds, call EV; see [decideAction contract](#decideaction-contract). |
| | `decideActionAsync(…, options?)` | Same as sync; Promise on libuv thread pool; optional `AbortSignal`. |
| | `encodePokerState(state)` | PKST packed bytes from `NativePokerState`. |
| | `decodePokerState(bytes)` | Round-trip to `NativePokerState`. |
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
| **Reverse implied / geometry** | `geometricPotAfterMatchedPotFractions(pot0, fraction, nRounds)` | Pot after `nRounds` of matched pot-fraction HU betting: `pot0 × (1 + 2f)^n`. |
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
| | `icmExpectedPayoutsWeitzman(stacks[], payouts[], alpha?, returnFormat?)` | Independent chip-utility ICM (`stack^alpha`, default `alpha=2`); not Harville. |
| | `icmPairwiseBubbleFactor(stacks[], payouts[], heroIndex, villainIndex, potChips)` | Loss/gain ratio from finite differences on `icmExpectedPayouts`. |
| **Side pots** | `sidePotLadderFromCommitments(committedChips[])` | Main + side layers; each layer `{ potChips, playerCapContribution[] }`. |
| | `layeredPotChipEvFromEquities(layerPotChips[], equityPlayerByLayer[][])` | Chip EV; each column sums to `1`. |
| | `sidePotLayersTotalChips(layers[])` | Sum of `potChips` across layers from `sidePotLadderFromCommitments`. |

### Tournament, exact runouts, and subgame helpers (23 functions)

| Group | Export | Role |
| --- | --- | --- |
| **Tournament ICM** | `icmHarvilleStackJacobian` | n×n ∂($EV)/∂(stack) via Harville. |
| | `icmHarvilleSkillAdjustedPayouts` | Harville with skill-tilted first-place weights. |
| | `icmChopNegotiationAnalysis` | Chip-chop vs ICM surplus + Pareto transfer pairs. |
| | `tournamentDuelAbsorptionProbabilities` | Closed-form gambler's ruin (stack-aware absorption, expected all-ins). |
| **Exact combinatorics** | `exactHeroRunoutVulnerability` / `Async` | p(nuts), p(dominated) over runouts. |
| | `exactVillainLeapfrogOutCounts` | Anti-out / hero-improve deck indices. |
| | `exactHeroCategoryJointFlopToRiver` | 9×9 joint category matrix on flop. |
| | `exactRangeDominatedComboFraction` | Weighted dominated combo share (river board). |
| | `exactHeroEquityRunoutQuantiles` / `Async` | Runout equity distribution quantiles. |
| | `exactEquityCardRemovalGradient` / `Async` | 52-vector equity sensitivity to dead cards. |
| **Subgame / range** | `materializeVillainRangeAfterBlockers` | Dense 1326 weights + entropy after blockers. |
| | `solveRiverPolarizedIndifferenceBet` | Closed-form polarized river bet: (1 − v)·pot/(2v − 1) makes a bluff-catcher indifferent. |
| | `exactInformationRegretVsClairvoyant` | Clairvoyant vs realistic call/fold EV gap. |
| | `solveSymmetricPushFoldThreshold` | Symmetric push/fold equity threshold with blinds/antes. |
| **Nash push/fold** | `nashHeadsUpJamRange` | HU Nash jam frequencies (169). Fictitious play, default 50 iters (cap 80). |
| | `nashHeadsUpCallRange` | HU Nash call frequencies vs jam (169), same order as `buildPreflopEquityMatrix`. |
| | `nashHeadsUpJamCallSolve` | Joint HU jam/call solve plus hero/villain EV and iteration count. |
| | `nashBlindVsBlindSolve` | SB jam/fold vs BB call/fold; SB blind is dead in the pot (no complete-or-jam). |
| | `nashJamFoldChart169` | Per-hand max stack in BB that still jams at Nash. |
| | `nashCallChart169` | Per-hand max stack in BB that still calls a jam at Nash. |
| | `nashIndifferenceStackBb` | Single-hand stack in BB where jam EV ≈ fold EV vs a Nash caller. |
| | `nashIcmHeadsUpJamCallSolve` | HU Nash with Harville ICM $EV (`otherStacks`, `payouts`). |
| **Suit isomorphism** | `canonicalFlopBoard` | Map any 3-card flop to its suit-canonical representative (1755 classes). |
| | `canonicalBoard` | Canonical 3–5 card board; flop set first, then turn/river. Re-solves S4. |
| | `canonicalHolesAndBoard` | Remap hero 2 + board; returns `suitPerm` for range relabel. |
| | `suitPermFromCanonicalFlop` | Length-4 suit map `perm[old]=new` (0=c … 3=s). |
| | `applySuitPermToCards` | Apply a suit perm to any card list (order preserved). |
| | `applySuitPermToRange1326` | Permute a dense 1326 range; total mass preserved. |
| | `isomorphicFlopOrbitSize` | Raw flops in this class (rainbow 24 / two-tone 12 / monotone 4; pairs smaller). |
| | `countCanonicalFlops` | Constant 1755. |
| | `isomorphicFlopIndex` | Canonical flop → stable index 0..1754. |
| | `flopIndexToCanonical` | Inverse of `isomorphicFlopIndex`. |

### CFR and best-response subgames (10 functions)

HU river check/bet tree: **bettor Check or Bet**; vs Check the defender checks back to showdown; vs Bet the defender **Fold or Call**. Showdown uses exact 7-card compare. Push-fold is jam/fold vs call/fold with stacks in BB (blinds 0.5/1). Not closed-form MDF/alpha toys.

| Group | Export | Role |
| --- | --- | --- |
| **CFR primitives** | `regretMatchingStrategy` | `max(r,0)/sum`; uniform if all regrets ≤ 0. |
| | `cfrNodeReachUpdate` | One info-set CFR step: `regret += reach * instantaneous`, then regret-match. |
| | `strategySupportSize` | Mixed combo count in `(eps, 1-eps)` plus pure jam/bet mass. |
| **River tree** | `cfrRiverBetCallFoldSolve` | Vanilla CFR on the HU river check/bet tree. Returns range-weighted bet/call freq, EV, 1326 mixes. |
| | `fictitiousPlayRiver` | Fictitious play on the same river tree. |
| | `evOfStrategyProfile` | Chip EV of a fixed bet/call mix. No solving. |
| | `bestResponseRiver` | Defender BR vs a fixed villain bet/check mix (scalar or per-combo). |
| | `exploitabilityRiver` | NashConv `0.5 * (BR0 + BR1 − EV0 − EV1)` of a river profile. |
| | `solveHuRiverCheckBetTree` | Full river CFR plus air/draw/made/strong freqs and top-k bet combos. |

### Bucketed flop CFR (10 functions)

HU flop abstraction: each 1326 range is mapped to **K** equal-width EHS2 buckets vs the opponent (default K = 20). Chance can be keyed by the 1755 canonical flop classes. Action tree is check/bet then fold/call, but **showdown leaves are bucket-vs-bucket**, not 7-card compares.

**Approximation:** `P(i beats j) = ehs_i / (ehs_i + ehs_j)` from each bucket's mean EHS2 (or the midpoint `(i+0.5)/K`). Combo-level blockers are dropped. This is how production solvers start — not a toy polarized river. Not a wrap/rename of `cfrRiverBetCallFoldSolve`.

| Group | Export | Role |
| --- | --- | --- |
| **Buckets** | `ehs2BucketsVsRange` | Hero 1326 → bucket id `0..K-1` (EHS2 vs the given range). Board-blocked combos are `-1`. |
| | `bucketMassFromRange` | Normalize a 1326 range into K masses (sum ≈ 1). |
| | `flopBucketCountDefault` | Default K (20). |
| **Flop tree** | `flopBucketStrategyTo1326` | Expand a length-K mix back to 1326 (uniform inside each bucket). |
| **Iso / hands** | `canonicalFlopCfrKey` | Canonical flop index + millichip pot/stack hash (suit-stable). |

### Alphabetical export index (370)

See [API reference](https://poker-calculations.devomb.com/docs/reference/api) for grouped tables with when-to-use notes. Maintainer check: `node scripts/list-native-exports.mjs` (expect count **370**).

## Card strings

| Rule | Detail |
| --- | --- |
| Ranks | `2`–`9`, `J`, `Q`, `K`, `A`; ten as `T` or `10` (canonical output uses `T`) |
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
| **`HandEvalResult`** | `rank: string`, `rankCategory: number`, `strength: number`, `kickers: number[]` (length 5) |
| **`HandEvalResultSlim`** | `rankCategory: number`, `strength: number` (`evaluateBestHand` with `{ format: 'slim' }`) |
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

## Examples and walkthroughs

Runnable samples and per-export guidance live on the **documentation site**, not in repo scripts:

- [Introduction & quick start](https://poker-calculations.devomb.com/docs/intro)
- [API reference](https://poker-calculations.devomb.com/docs/reference/api) (grouped by the same categories as the tables above)

To print every N-API export at runtime (maintainers): `node scripts/list-native-exports.mjs`.

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
| Nash push/fold | [`include/poker/nash_push_fold.hpp`](include/poker/nash_push_fold.hpp) | Two-player fictitious-play jam/call Nash (169), first-in / multiway first-caller, ICM $EV, indifference stacks. |
| PKO / bounty | [`include/poker/pko.hpp`](include/poker/pko.hpp) | Covering knockout matrix, ICMBU, mystery/progressive bounty $EV. |
| FGS / ICM decisions | [`include/poker/fgs.hpp`](include/poker/fgs.hpp) | Average-position FGS, jam/call $EV, stalling, pay-jump survival. |
| Exact multiway | [`include/poker/exact_multiway.hpp`](include/poker/exact_multiway.hpp) | Exact 3–6 way known-hand equity, side-pot chip EV, runout extremes. |
| CFR subgame | [`include/poker/cfr_subgame.hpp`](include/poker/cfr_subgame.hpp) | River check/bet CFR, fictitious play, exploitability, HU push-fold CFR. |
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

---

## PKO / bounty

Progressive knockout and mystery-bounty $EV. Covering model: P(j busts) is Harville last-place among players with chips (`icmLastPlaceProbabilitiesHarville` on the alive subset); P(i knocks j | j busts) = `stack_i / (total − stack_j)` when i covers j (`stack_i >= stack_j`), else 0. Diagonal 0. Zero bounties match freezeout ICM. n in 2..31.

| Group | Export | Role |
| --- | --- | --- |
| **PKO / bounty** | `pkoKnockoutProbabilityMatrix(stacks[])` | Flat n×n `Float64Array` + `n`; P(i collects j's bounty). |
| | `pkoExpectedBountyCollection(stacks[], bountyValues[])` | E[bounty $] per seat. |
| | `pkoIcmbuPayouts(stacks[], payouts[], bountyValues[])` | Freezeout ICM + expected bounty collection. |
| | `pkoBountyRiskPremium(stacks[], payouts[], bountyValues[])` | Freezeout vs ICMBU; chip-share of bounty pool vs expected collection. |
| | `pkoCallEvVsShove(stacks[], payouts[], bountyValues[], hero, villain, pot, equity)` | $EV(call all-in) vs $EV(fold) with KO bounty. |
| | `pkoJamEvVsFold(..., foldEquity, equityWhenCalled)` | $EV(jam) vs $EV(fold) including bounties. |
| | `mysteryBountyExpectedValue(values[], weights?, k?)` | Weighted mean of one prize; leftover-pool sum; optional k-draw EV. |
| | `progressiveKoPostedBounty(baseBounties[], knockouts, carryFraction)` | Posted bounty-on-head after progressive carry. |
| | `pkoCoveringHuntEv(..., hunter, prey, pot, equity?)` | Isolate vs a covered short stack vs check-down. |
| | `pkoWinnerTakeRemainingBounties(stacks[], payouts[], remainingBountyPool)` | Leftover bounty pool added to the winner's first prize. |

---

## Future Game Simulation and ICM decisions

Average-position FGS and $EV decision helpers. They call existing Harville `icmExpectedPayouts` (and `orbitCostChips`) — they do not wrap or replace pairwise bubble factor, Shapley, Jacobian, or chop exports. Busted seats get $0; remaining seats take the top-k prizes. Paid blinds in FGS leave the table (dead pool), not a specific seat.

| Export | Role |
| --- | --- |
| `icmPayoutsAfterBlindPost(stacks, payouts, hero, heroPost, posts)` | Subtract posts (hero uses `heroPost`), pot is dead, ICM on remaining stacks. |
| `icmCallingBubbleFactor(...)` | `(EV_now − EV_lose) / (EV_win − EV_now)` for one all-in; distinct from `icmPairwiseBubbleFactor`. |
| `fgsPayoutsBlindSchedule(stacks, payouts, sb[], bb[], ante[], orbitsAtLevel[])` | Walk a blind schedule, then ICM. |
| `icmDeadPotDollarEv(...)` | Two-point $EV of winning a dead pot (hero stack += dead chips). |

---

## Omaha Hi (PLO)

Native 4-card Omaha Hi. A made hand is **exactly 2 hole + exactly 3 board** (flop `C(4,2)×C(3,3)=6`, river `C(4,2)×C(5,3)=60`). Hold'em 5-card evaluation runs only on that chosen five — not a 7-card best-of-9 from nine cards. C++: [`include/poker/omaha.hpp`](include/poker/omaha.hpp), [`src/omaha.cpp`](src/omaha.cpp), [`native/binding_omaha.cpp`](native/binding_omaha.cpp). Smoke: `node scripts/verify-omaha.mjs`.

| Export | Role |
| --- | --- |
| `evaluateOmahaBestHand(hole, board, options?)` | Best Omaha 5-card hand. Hole 4, board 3–5. Same `HandEvalResult` as `evaluateBestHand`. |
| `evaluateOmahaHandStrength(hole, board)` | Same `pack_hand_strength` uint64 layout as Hold'em 5-card strength. |
| `exactHuOmahaEquityVsKnown(hero, villain, board)` | Exact HU vs known 4-card hand. Board 0–5; remaining runouts enumerated. |
| `simulateOmahaEquityVsRandom(hero, board, trials, seed)` | MC vs uniform random 4-card villain. |
| `simulateOmahaEquityVsRange(hero, board, range, trials, seed)` | MC vs sparse `{ packed, weights? }`. `packed`: 4 deck ids (0..51) per combo. Not a dense 270725 vector. |
| `omahaComboCount(dead)` | `C(52 − \|unique dead\|, 4)`. Throws on duplicate dead cards. |
| `omahaNutsOnBoard(hero, board, extraDead?)` | True if no other 4-card combo beats hero on this board. |
| `omahaWrapDrawOuts(hero, flop)` | `{ outs, nutOuts }`. Next cards that make a straight (or SF/royal) via 2+3. `nutOuts` = those that are the nuts on the 4-card board. Flop only. |
| `omahaNuttednessScore(hero, board, extraDead?)` | `1 − (better holdings) / (n − 1)` among legal 4-card Omaha holdings. Unique nuts → 1. |
| `omahaMultiwayEquityMc(holes, board, trials, seed)` | MC pot-share equity for 3–4 known 4-card hands. |

**Range spec:** `{ packed: Uint8Array \| number[], weights?: number[] \| Float64Array }`. Length of `packed` is `4n`; each group of four bytes is one combo. Omitted weights are 1. Combos that collide with hero/board are skipped at sample time.

**Wrap model:** a flop out is a remaining card that, added as the turn, makes hero’s best Omaha hand a straight, straight flush, or royal (not a made flush/boat that is not a straight).

## Big O (5-card PLO)

Native 5-card Big O. Same 2-hole + 3-board made-hand rule as Omaha Hi; the extra hole card only adds pairs (`C(5,2)×C(5,3)=100` on the river). C++: [`include/poker/big_o.hpp`](include/poker/big_o.hpp), [`src/big_o.cpp`](src/big_o.cpp), [`native/binding_big_o.cpp`](native/binding_big_o.cpp). Reuses `plo_best_two_plus_three` from Omaha. Smoke: `node scripts/verify-big-o.mjs`.

| Export | Role |
| --- | --- |
| `evaluateBigOBestHand(hole, board, options?)` | Best Big O 5-card hand. Hole 5, board 3–5. Same `HandEvalResult` as `evaluateBestHand`. |
| `evaluateBigOHandStrength(hole, board)` | Same `pack_hand_strength` uint64 layout as Hold'em 5-card strength. |
| `exactHuBigOEquityVsKnown(hero, villain, board)` | Exact HU vs known 5-card hand. Board 0–5; remaining runouts enumerated. |
| `simulateBigOEquityVsRandom(hero, board, trials, seed)` | MC vs uniform random 5-card villain. |
| `simulateBigOEquityVsRange(hero, board, range, trials, seed)` | MC vs sparse `{ packed, weights? }`. `packed`: 5 deck ids (0..51) per combo. |
| `bigOComboCount(dead)` | `C(52 − \|unique dead\|, 5)`. Throws on duplicate dead cards. |
| `bigONutsOnBoard(hero, board, extraDead?)` | True if no other 5-card combo beats hero on this board. |
| `bigOMultiwayEquityMc(holes, board, trials, seed)` | MC pot-share equity for 3–4 known 5-card hands. |

**Range spec:** `{ packed: Uint8Array \| number[], weights?: number[] \| Float64Array }`. Length of `packed` is `5n`; each group of five bytes is one combo. Omitted weights are 1. Combos that collide with hero/board are skipped at sample time.

## MTT / table spots

Spin & Go, late-reg overlay, satellite tickets, and pot-geometry spots that reuse Harville / FGS / PKO / Nash. They do not re-export the 3.1.1 PKO, FGS, Nash, or CFR names.

| Export | Role |
| --- | --- |
| `spinGoPayouts(multiplier, buyin, winnerTakeAll?)` | Length-3 prize vector. Default 50/30/20 of `multiplier * buyin`; WTA is 100/0/0. |
| `spinGoIcmEv(stacks, payouts)` | Harville ICM $EV for three stacks (`icmExpectedPayouts`). Equal stacks + 50/30/20 → equal $EV. |
| `pkoFgsPayouts(stacks, payouts, bounties, orbits, sb, bb, ante?)` | FGS orbits on stacks, then ICMBU on survivors. `orbits=0` matches `pkoIcmbuPayouts`. |
| `squeezeEv(pot, heroPut, openerCall, callerCall, feOpener, feCaller, eqOp, eqCaller, eqBoth)` | Chip EV of squeeze vs fold (fold = 0). Independent folds; continue pots add matching calls. |
| `fourBetJamEv(deadPot, jam, call, foldEquity, equityWhenCalled)` | 4-bet jam pot geometry (not ICM). `FE=1` wins dead money. |
| `threeBetPotCommitEv(pot, remaining, equity, realization?)` | SPR after 3-bet, stack-off vs realized equity, continue EV vs fold 0. |

## Short Deck (6+ Hold'em)

36-card deck, ranks **6–A**. Card ids keep the NLHE layout (`rank * 4 + suit`); ranks 2–5 are rejected. Flush **beats** full house. Wheel is **A6789** (nine-high); A2345 does not exist. Broadway T-J-Q-K-A and royal flushes are unchanged. Preflop classes: **81** (9 pairs + 36 suited + 36 offsuit), not 169 and not 91. `Float64Array(169)` is accepted only when 2–5 class weights are 0.

`rank` strings match NLHE labels (`flush`, `fullHouse`). `rankCategory` / packed strength use 6+ order: `straight=4`, **`fullHouse=5`, `flush=6`**, `fourOfAKind=7`. A single hand that is a flush is still called `flush` in both games; the swap only changes which hand wins at showdown. Category *labels* differ on the wheel (NLHE high card or flush vs 6+ straight / straight flush).

| Export | Role |
| --- | --- |
| `evaluateShortDeckBestHand(cards)` | Best five of 1–7 short-deck cards. |
| `evaluateShortDeckHandStrength(hole, board)` | Packed 6+ strength (flush > boat). |
| `evaluateShortDeckCategory(hole, board)` | Category label under 6+ ranking. |
| `exactHuShortDeckEquityVsKnown(hero, villain, board)` | Exact HU on remaining 36-card runouts; board 0–5. |
| `simulateShortDeckEquityVsRandom(hero, board, n, seed)` | Monte Carlo vs a random 6+ hand. |
| `simulateShortDeckEquityVsRange(hero, board, range, n, seed)` | Monte Carlo vs an 81-class (or zero-padded 169) range. |
| `shortDeckStraightIsWheel(cards)` | Five-card A6789 wheel (straight or steel wheel). |
| `shortDeckRemainingComboCount(dead)` | `C(n,2)` hole combos left on the 36-card deck. |
| `shortDeckNashHuJamRange(stackBb \| options)` | HU jam/fold Nash frequencies, length 81, stacks in BB. |
| `shortDeckVsHoldemCategoryFlip(cards)` | True when NLHE vs 6+ category labels differ for the same 5–7 cards. |

## Hand potential (HS / PPot / NPot / EHS / EHS2)

Classic Billings / poker-eval / Casinostates primitives vs a villain range (sparse or dense 1326). Board is flop or turn. Ties follow NUMERICAL.md HU chop (half). Blocked combos are removed.

| Export | Role |
| --- | --- |
| `handStrengthVsRange(heroHole, board, range)` | HS = P(ahead now) + 0.5 P(tie) if the hand ended on this board. |
| `positivePotentialVsRange(heroHole, board, range)` | One-card PPot (flop→turn or turn→river). |
| `negativePotentialVsRange(heroHole, board, range)` | One-card NPot. |
| `effectiveHandStrength(heroHole, board, range)` | EHS = HS×(1−NPot) + (1−HS)×PPot. |
| `effectiveHandStrengthSquared(heroHole, board, range)` | EHS2 = HS×(1−NPot)² + (1−HS)×PPot². |
| `handPotentialBreakdown(heroHole, board, range)` | `{hs, ppot, npot, ehs, ehs2, nBehind, nAhead, nTied}` in one pass. |
| `twoStreetPositivePotential(heroHole, flop, range)` | Flop→river PPot (two cards). |
| `twoStreetNegativePotential(heroHole, flop, range)` | Flop→river NPot. |
| `equityBucketFromEhs(ehs, k)` | Equal-width bucket in `[0, k)` for EHS in `[0, 1]`. |
| `comboEhsTableVsRange(board, range[, { trials, seed }])` | EHS for all 1326 hero combos (0 if blocked). Turn is exact when `trials` is omitted/0. Flop exact is allowed but heavy; pass `trials` to Monte Carlo next-street cards. |

## Omaha Hi-Lo (PLO-8)

Native 4-card Omaha 8-or-better. A made hand is still **exactly 2 hole + exactly 3 board**. High is PLO high (`evaluateOmahaBestHand`). Low is five unpaired ranks 8 or lower; Ace is low; straights and flushes do **not** count against the low. Scoop / quartering: hi half + lo half; if nobody qualifies, high takes the whole pot. C++: [`include/poker/omaha_hi_lo.hpp`](include/poker/omaha_hi_lo.hpp), [`src/omaha_hi_lo.cpp`](src/omaha_hi_lo.cpp), [`native/binding_omaha_hi_lo.cpp`](native/binding_omaha_hi_lo.cpp). Smoke: `node scripts/verify-omaha-hi-lo.mjs`.

| Export | Role |
| --- | --- |
| `evaluateOmahaLoHand(hole, board)` | Best qualifying 8-or-better low, or `{ qualifies: false }`. Hole 4, board 3–5. `ranks` high-to-low, Ace=1. |
| `omahaLoQualifies(hole, board)` | True iff that low qualifies. |
| `evaluateOmahaHiLo(hole, board)` | `{ hi, lo }`. `hi` is the same `HandEvalResult` as `evaluateOmahaBestHand`. |
| `exactHuOmahaHiLoEquity(hero, villain, board)` | Exact HU: `hiEquity`, `loEquity`, `scoopEquity`, `quarterRate`, `potShare`. Board 0–5. |
| `simulateOmahaHiLoEquity(hero, villain\|null, board, trials, seed)` | Monte Carlo vs a known 4-card hand or `null` (uniform random). |
| `omahaScoopProbabilityMc(hero, villain, board, trials, seed)` | P(scoop) vs known villain holes. |
| `omahaQuarterProbabilityMc(hero, villain, board, trials, seed)` | P(split exactly one side, win or lose the other). |
| `omahaLoNutsOnBoard(hero, board, extraDead?)` | True if hero’s low is unbeaten by every other 4-card combo. |
| `omahaHiLoNuttedness(hero, board, extraDead?)` | `{ hiNuts, loNuts, scoopNuts }`. `scoopNuts` if nut high and nut low, or nut high when no low is possible. |
| `omahaHiLoMultiwayMc(holes, board, trials, seed)` | 3-way chip EV with quartering. Exactly 3 known 4-card hands. |

## 2-7 single draw (Kansas City lowball)

Native 5-card 2-7 lowball, one draw. **Ace is high** — A-5-4-3-2 is Ace-high, not a wheel. Straights and flushes **count against you**. Best hand is 7-5-4-3-2 rainbow. Packed strength uses the same `pack_hand_strength` bit layout as Hold'em; **lower is better**. After the draw a player discards 0–5 and replaces from the remaining deck. C++: [`include/poker/deuce_seven.hpp`](include/poker/deuce_seven.hpp), [`src/deuce_seven.cpp`](src/deuce_seven.cpp), [`native/binding_deuce_seven.cpp`](native/binding_deuce_seven.cpp). Smoke: `node scripts/verify-deuce-seven.mjs`.

| Export | Role |
| --- | --- |
| `evaluateDeuceSevenHand(cards)` | Packed 2-7 strength of five cards. Lower integer wins. |
| `evaluateDeuceSevenCategory(cards)` | `nuts` / `smooth` / `rough` / `number` / `paired` / `twoPair` / `trips` / `straight` / `flush` / `fullHouse` / `quads` / `straightFlush`. 7- and 8-high unpaired: `smooth` if the second card is not consecutive, else `rough`. 9+ unpaired: `number`. 75432 rainbow: `nuts`. |
| `deuceSevenIsPat(cards, eightPat?)` | Unpaired, no straight, no flush. Default `eightPat=true` counts 8-high as pat; `false` requires 7-high. |
| `deuceSevenDrawEquityVsKnown(hero, villain, options?)` | Hero equity after both stand or draw. Options: discard cards or count, or keep cards. Exact replacement tree when small; else MC (`trials`, `seed`). |
| `deuceSevenNutsPat(cards)` | True iff 7-5-4-3-2 unpaired unsuited. |
| `deuceSevenRoughVsSmooth(a, b)` | Both must be unpaired 8-high. `{ cmp, aSmooth, bSmooth }`. `cmp` is -1 if `a` is better 2-7. |
| `deuceSevenMultiwayShowdown(hands)` | Pot-share vector for 2–8 disjoint 5-card hands. Ties split. |

## 7-card stud hi / razz

Stud hi: 7 cards, best 5-card high (reuses the Hold'em evaluator). Razz: 7-card A-to-5 low. Aces low. Straights and flushes do **not** count. Best hand is A2345 (wheel). Distinct from 2-7 lowball. C++: [`include/poker/stud_razz.hpp`](include/poker/stud_razz.hpp), [`src/stud_razz.cpp`](src/stud_razz.cpp), [`native/binding_stud_razz.cpp`](native/binding_stud_razz.cpp). Smoke: `node scripts/verify-stud-razz.mjs`.

| Export | Role |
| --- | --- |
| `evaluateStudBestHand(cards, options?)` | Best 5-card high from 3–7 cards. Same `HandEvalResult` as `evaluateBestHand`. |
| `evaluateRazzHand(cards, options?)` | Best 5-card A-5 low from 3–7 cards. Lower `strength` is better. |
| `razzWheelIsNuts(cards)` | True iff the best 5 is A2345. Suited wheel still counts. |
| `exactHuStudEquityVsKnown(hero, villain, extraDead?)` | Exact HU stud. Both 7 → compare. Fewer → enumerate remaining streets from the dead-aware deck. Ties 0.5. |
| `exactHuRazzEquityVsKnown(hero, villain, extraDead?)` | Same dealing, A-5 low. |
| `studDeadCardDeck(dead)` | Remaining canonical cards after holes + upcards. |
| `simulateStudEquityVsRandom(hero, trials, seed, extraDead?)` | MC: random matching-street villain, complete both to 7. |
| `simulateRazzEquityVsRandom(hero, trials, seed, extraDead?)` | MC razz, same runout model. |

---

*Last verified: **370** native functions in `binding_register.cpp` / `index.d.ts`. Re-run `node scripts/list-native-exports.mjs` after adding bindings.*

