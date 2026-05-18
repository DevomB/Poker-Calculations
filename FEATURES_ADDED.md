# Math features — shipped inventory

This document lists **every math-related capability** currently shipped in `poker-calculations`.

Authoritative public API: [`index.d.ts`](index.d.ts) and the API tables in [`README.md`](README.md).

---

## JavaScript / N-API exports (from `index.d.ts`)

These are the symbols consumers `require('poker-calculations')` receive. All are implemented in C++ and bound in [`native/binding.cpp`](native/binding.cpp). This table is the **entire** JavaScript API (see also `examples/native-binding-exports.mjs` for an auto-generated list at runtime). **C++-only** engine pieces (full `GameEngine`, deck lifecycle, `BotConfig` file I/O) are summarized under [Engine and integration](#engine-and-integration-from-readme-features-engine) below and live outside this export list.

| Group | Export | Role |
| --- | --- | --- |
| **Hand resolution** | `evaluateBestHand(cards)` | Best five of up to seven cards; returns rank name + kicker vector. |
| | `evaluateHandStrength(holeCards, board)` | Scalar / categorical strength string for hero cards vs board. |
| | `evaluateHandCategory(holeCards, board)` | High-level category label. |
| | `validateCardString(card)` | `true` if `card` parses as one card (`Ah`, `10c`, …). |
| | `cardStringsHaveDuplicate(cards[])` | `true` if two entries parse to the same card; throws if any string is invalid. |
| | `compareBestHands(cardsA[], cardsB[])` | `-1` / `0` / `1` best-hand order; throws on overlap or invalid cards. |
| | `canonicalCardString(card)` | Canonical two-character card (`Th`, `Ac`, …); throws if invalid. |
| | `parseCompactCardList(text)` | Parse concatenated or whitespace-separated cards (`AhKh`, `10h Kd`); throws on duplicate or invalid token. |
| | `handRankCategoryOrder(category)` | Integer order `0..9` for `evaluateHandCategory` labels (`highCard` … `royalFlush`). |
| **Monte Carlo equity** | `simulateHandOutcome(holeCards, board, numSimulations, seed, villains?)` | Estimated equity vs one or more random villain hands. |
| | `parallelHandSimulation(holeCards, board, numSimulations, baseSeed, villains, numThreads)` | Same idea with worker threads and distinct seeds. |
| | `exactHuEquityVsRandomHand(heroHoleCards, boardCards)` | **P22** Exact HU equity vs uniform random villain hand; board must have **3–5** cards (full runout enumeration). |
| **Strategy** | `decideAction(state, config, opponentModel?, heroSeat?)` | Rule-based action using MC equity (or strength fallback when sim count is 0), pot odds, call EV. |
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
| | `breakevenCallEquityWithRake(potBeforeCall, toCall, rakeFraction, rakeCap)` | **P9** Breakeven equity when the **final** pot (after call) pays rake under that model. |
| **Stacks & display** | `spr(potChips, effectiveStackChips)` | Stack-to-pot ratio. |
| | `effectiveStack(...stacks)` | Minimum stack; empty → `0`. |
| | `normalizedStackFractions(stacks[])` | Each stack divided by sum of stacks (chip shares; not Harville ICM). |
| | `stackInBigBlinds(stackChips, bigBlind)` | Stack size in big blinds. |
| | `potOddsRatioDisplay(potBeforeCall, toCall)` | Display ratio `pot : to_call` (e.g. `3.5` means 3.5:1). |
| | `formatPotOdds(potBeforeCall, toCall, decimals?)` | Human-readable `"x:1"` string. |
| | `harringtonM(stackChips, smallBlind, bigBlind, totalAntes)` | **P11** Harrington **M** = stack / (sb + bb + antes). |
| | `harringtonMEffective(stackChips, smallBlind, bigBlind, antePerActivePlayer, numActivePlayers)` | **P11** Effective M = stack / (sb + bb + antes from active players only). |
| | `harringtonMEffectiveActiveAntes(stackChips, smallBlind, bigBlind, antesFromActiveSeats[])` | **P11** Same denominator with explicit per-seat antes for active players only. |
| | `harringtonQ(heroStack, stacks[])` | Harrington **Q** = hero stack / mean(table stacks); all stacks positive. |
| | `orbitCostChips(smallBlind, bigBlind, antesFromSeats[])` | One orbit posted cost: sb + bb + sum of antes. |
| | `nlMinimumRaiseToTotal(currentMaxWager, lastRaiseIncrement, bigBlind)` | NL toy minimum **total** wager after a raise: current + `max(last increment, BB)`. |
| | `preflopCombosFromNotation(notation)` | NLHE combo count from shorthand (`AA`, `AKs`, `AKo`). |
| | `preflopCombosFromNotationsList(notations[])` | Sum of combo counts over a list; empty → `0`. |
| **Heuristics** | `ruleOfFourEquity(outs)` | Out-count × 4% cap heuristic (turn+river). |
| | `ruleOfTwoEquity(outs)` | Out-count × 2% cap heuristic (one card). |
| | `estimatedOutsFromRuleOfTwo(equity, unseenCards)` | Uncapped inverse-style estimate `equity×unseen/2` clamped to `[0, unseen]` (not exact inverse of capped rule-of-two). |
| | `estimatedOutsFromRuleOfFour(equity, unseenCards)` | Same for two streets: `equity×unseen/4` clamped. |
| | `impliedBreakevenFutureWin(potBeforeCall, toCall, equity)` | Average extra future win needed for a neutral call; `+∞` if equity ≤ 0. |
| | `hypergeometricOneCardHitProbability(outs, unseenCards)` | **P1** One-card draw: `outs / unseenCards`. |
| | `runnerRunnerBackdoorFlushTwoCardProbability(suitCardsRemaining, unseenCards)` | **P3** `C(s,2)/C(u,2)` for two-card runner flush. |
| | `flopToRiverAtLeastOneHitProbability(outs, unseenAfterFlop)` | **P2** Two streets, single effective out count, no hit on both misses. |
| | `flopToRiverAtLeastOneHitUnionTwoCategories(unseenAfterFlop, outsA, outsB, sharedAb)` | **P2** Two categories with overlap; union cardinality in the two-draw formula. |
| | `flopToRiverAtLeastOneHitUnionThreeCategories(...)` | **P2** Three categories; inclusion–exclusion on union size, then same two-street formula. |
| | `flopToRiverAtLeastOneHitUnionFourCategories(...)` | **P2** Four categories; full inclusion–exclusion on card-count intersections, then same two-street formula. |
| | `flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop, outsPerCategory[])` | **P2b** Sum **disjoint** categories then same as P2 (categories must not share outs). |
| | `runnerRunnerStraightDrawHitProbability(straightKind, deadAmongPatternOuts, unseenAfterFlop)` | **P4 (pattern)** Runner–runner straight draw: gutshot (4) / OESD or double-belly (8), minus dead among pattern outs. |
| | `straightMadeFlopToRiverExactProbability(heroHoleCards[], flopThree[], knownDead[])` | **P4 (enumeration)** Exact P(straight or better in hero’s best 7) after uniform random unordered turn+river from remaining deck. |
| | `duplicationAdjustedOuts(outs, numVillains, duplicationWeight)` | **P25** Heuristic `outs / (1 + weight × villains)`. |
| **Reverse implied / geometry** | `reverseImpliedOddsMaxFutureLoss(potBeforeCall, toCall, equity)` | **P6** Toy ceiling on extra future loss when losing while keeping the call ≥ 0 EV. |
| | `geometricPotAfterMatchedPotFractions(pot0, fraction, nRounds)` | **P7** Pot after `nRounds` of matched pot-fraction HU betting: `pot0 × (1 + 2f)^n`. |
| **Stats & risk** | `monteCarloStandardError(pHat, nTrials)` | **P15** Binomial SE `√(p̂(1−p̂)/n)`. |
| | `monteCarloTrialsForStandardErrorBound(pHat, targetSe)` | Smallest integer `n` with SE ≤ `targetSe` at interior `pHat` (ceil of `p(1−p)/se²`). |
| | `wilsonScoreInterval(successes, nTrials, z)` | **P16** Wilson interval; returns `{ lower, upper }`. |
| | `agrestiCoullInterval(successes, nTrials, z)` | Agresti–Coull interval (same `{ lower, upper }` shape). |
| | `normalWaldBinomialInterval(successes, nTrials, z)` | Normal (Wald) `p̂ ± z·SE` clamped to `[0,1]` (weak near 0/1 with small `n`). |
| | `monteCarloTrialsForHoeffdingBound(epsilon, delta)` | Hoeffding: smallest `n` with `n ≥ ln(2/δ)/(2ε²)` for uniform MC error bound. |
| | `riskOfRuinDiffusionApprox(driftPerHand, variancePerHand, bankroll)` | **P13** `exp(−2μB/σ²)` style ROR; returns `1` if drift ≤ 0. |
| | `bankrollForTargetRorDiffusion(driftPerHand, variancePerHand, targetRor)` | **P14** Inverse of P13 for bankroll `B`. |
| | `betaBinomialFoldPosterior(priorAlpha, priorBeta, folds, calls)` | **P24** Conjugate Beta update; returns `{ alpha, beta, posteriorMean }`. |
| **Kelly & jam toys** | `kellyCriterionBinary(winProbability, netOdds)` | **P12** Full Kelly `(p·b − (1−p)) / b` for net odds `b`. |
| | `chubukovSymmetricJamBreakevenStack(deadMoneyChips, equity)` | **P23** Toy symmetric jam: `S = equity·dead/(1−2·equity)` for `equity < 0.5`; `+∞` if `equity > 0.5`. |
| | `chubukovSymmetricJamEv(jamStackChips, deadMoneyChips, equity)` | **P23** Symmetric jam toy EV in chips. |
| | `chubukovMaxSymmetricJamStackChipsBinarySearch(equity, deadMoneyChips, maxStackChips)` | **P23** Largest integer jam stack in `[1, max]` with nonnegative EV for fixed equity. |
| | `chubukovMaxSymmetricJamStackBinarySearch(heroHoleCards[], boardCards[], deadMoneyChips, maxStackChips)` | **P23** Same as chips search on equity from `exactHuEquityVsRandomHand` (board 3–5); `maxStackChips` clamped like native `double` → int cap. |
| | `chubukovMaxSymmetricJamStackFromHandBinarySearch(heroHoleCards[], boardCards[], deadMoneyChips, maxStackChips)` | **P23** Same integer search; `maxStackChips` read as **int32** in the binding (pair with the other export for large caps). |
| **GTO-style (toy)** | `minimumDefenseFrequency(potBeforeOpponentBet, opponentBetSize)` | MDF from pot geometry. |
| | `alphaFrequency(potBeforeBet, betSize)` | `1 - MDF` = exploit weight if hero never defends. |
| | `bluffToValueRatio(potBeforeBet, betSize)` | Polarized river combo ratio `bet / (pot + 2×bet)`. |
| | `valueToBluffRatio(potBeforeBet, betSize)` | Reciprocal; `Infinity` when bet is 0. |
| **Sizing & commitment** | `betAsPotFraction(potBeforeBet, betSize)` | Bet as fraction of pot. |
| | `sprAfterCall(potBeforeCall, toCall, effectiveStackBeforeCall)` | SPR after HU single call; throws if `toCall` > stack. |
| | `commitmentRatio(toCall, effectiveStackBeforeCall)` | Fraction of stack put in to call. |
| **Fold equity** | `breakevenFoldEquityPureBluff(potBeforeHeroBet, heroBetOrCallSize)` | FE when equity if called is 0. |
| | `breakevenFoldEquitySemiBluff(potBeforeHeroBet, heroBetSize, equityWhenCalled, totalPotIfCalled)` | Two-outcome model; may exceed 1 if line is −EV even if villain always folds. |
| | `breakevenFoldEquitySemiBluffWithRake(..., rakeFraction, rakeCap)` | **P10** Semi-bluff FE with rake on `totalPotIfCalled`. |
| | `breakevenFoldEquityPureBluffWithRake(...)` | **P10** Pure-bluff FE parallel to semi-bluff rake model. |
| | `twoStreetPureBluffSameFoldEquity(potBeforeStreet1, betStreet1, betStreet2)` | **P8** Same FE both streets, pure air; may return `NaN`. |
| | `twoStreetPureBluffEv(..., foldEquityStreet1, foldEquityStreet2)` | **P8** Two-street pure-bluff chip EV with independent FE per street. |
| | `breakevenFoldEquitySecondStreetPureBluff(..., foldEquityStreet1)` | **P8** Breakeven second-street FE given first-street FE. |
| | `breakevenFoldEquityFirstStreetPureBluff(..., foldEquityStreet2)` | **P8** Breakeven first-street FE given second-street FE. |
| **Multiway** | `multiwaySymmetricBreakevenCallEquity(potBefore, toCall, symmetricExtraCallers)` | **P5** `k` extra symmetric callers. |
| | `multiwaySymmetricBreakevenCallEquityWithShare(..., shareModel, heroFractionWhenWin)` | **P5** Same geometry with winner-take-all vs fixed hero share when winning (chop proxy). |
| **ICM** | `icmWinProbabilitiesHarville(stacks[])` | **P17** Harville first-place probabilities. |
| | `icmHarvillePlacementProbabilities(stacks[])` | **P17** Full `n×n` Harville placement matrix (per player, per finish rank). |
| | `icmTopKFinishProbabilities(stacks[], k)` | Sum of Harville placement over first `k` finish ranks per player (convenience on **P17**). |
| | `icmLastPlaceProbabilitiesHarville(stacks[])` | Harville probability each player finishes **last** (placement matrix last column). |
| | `icmExpectedPayouts(stacks[], payouts[])` | **P18** Expected payout per seat. |
| | `icmPairwiseBubbleFactor(stacks[], payouts[], heroIndex, villainIndex, potChips)` | **P19** Loss/gain ratio from finite differences on P18. |
| **Side pots** | `sidePotLadderFromCommitments(committedChips[])` | **P20** Main + side layers; each layer `{ potChips, playerCapContribution[] }`. |
| | `layeredPotChipEvFromEquities(layerPotChips[], equityPlayerByLayer[][])` | **P21** Chip EV; each column sums to `1`. |
| | `sidePotLayersTotalChips(layers[])` | Sum of `potChips` across layers from `sidePotLadderFromCommitments`. |

Supporting types: `NativePokerState`, `NativeBotConfig`, `NativeOpponentModel`, `HandEvalResult`, `DecisionResult`, `WilsonScoreInterval`, `BetaBinomialFoldPosterior`, `SidePotLayer`.

---

## C++ modules

| Module | Headers | Role |
| --- | --- | --- |
| Core chip / odds / probability | [`include/poker/poker_math.hpp`](include/poker/poker_math.hpp) | Pot odds, MDF, fold FE, heuristics, P1–P16 (except ICM), P23 toy jam + P2/P4/P5/P8/P9/P10/P11, rake helpers, NL orbit / Q / min-raise / preflop combo toys + list sum, inverse rule-of-2/4 outs, MC trial planner (SE + Hoeffding), Wilson / Agresti–Coull / Wald intervals, pot-odds display ↔ breakeven equity, reduced-fraction pot odds, equity ↔ winning odds-against, normalized stack shares, `hand_rank_category_order`. |
| Card strings | [`include/poker/card_string.hpp`](include/poker/card_string.hpp) | Shared parse + duplicate detection; `canonical_card_string`, `parse_compact_card_list`. |
| ICM | [`include/poker/icm.hpp`](include/poker/icm.hpp) | Harville full placement matrix, win probs, top‑k finish sums, last-place probabilities, $EV, bubble factor (**P17–P19**). |
| Side pots | [`include/poker/side_pot.hpp`](include/poker/side_pot.hpp) | Side-pot ladder, layered EV (**P20–P21**), `side_pot_layers_total_chips`. |
| Exact HU | [`include/poker/exact_equity.hpp`](include/poker/exact_equity.hpp) | Enumeration equity vs random hand (**P22**); exact flop→river straight-or-better (**P4**); Chubukov max integer jam stack from hand (**P23**). |

---

## Engine and integration (from README “Features (engine)”)

Not separate “formula” exports, but primitives future math can build on:

| Area | Included |
| --- | --- |
| **Cards / deck** | 52-card deck, shuffle with injected `std::mt19937`, deal, burn on board deals in `GameEngine`. |
| **State & rules** | `PokerGameState`, blinds, pot, per-street commits, phase machine (pre-flop → river → showdown), `GameEngine::apply_action` with `Decision`. |
| **Evaluation** | Best five of up to seven cards, full ranking + kickers, `evaluate_hand_strength` scalar. |
| **Strategy** | `decide_action` with `BotConfig`, optional `OpponentModel*`. |
| **Simulation** | `simulate_hand_outcome`, `parallel_hand_simulation`. |
| **Config** | `BotConfig::load_from_config_file` / `save_to_config_file` (`key=value`, `#` comments). |

---

*Verify against `index.d.ts` and `README.md` before relying on this list.*
