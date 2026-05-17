# Math features — shipped inventory

This document lists **every math-related capability** currently shipped in `poker-calculations`.

Authoritative public API: [`index.d.ts`](index.d.ts) and the API tables in [`README.md`](README.md).

---

## JavaScript / N-API exports (from `index.d.ts`)

These are the symbols consumers `require('poker-calculations')` receive. All are implemented in C++ and bound in [`native/binding.cpp`](native/binding.cpp).

| Group | Export | Role |
| --- | --- | --- |
| **Hand resolution** | `evaluateBestHand(cards)` | Best five of up to seven cards; returns rank name + kicker vector. |
| | `evaluateHandStrength(holeCards, board)` | Scalar / categorical strength string for hero cards vs board. |
| | `evaluateHandCategory(holeCards, board)` | High-level category label. |
| **Monte Carlo equity** | `simulateHandOutcome(holeCards, board, numSimulations, seed, villains?)` | Estimated equity vs one or more random villain hands. |
| | `parallelHandSimulation(holeCards, board, numSimulations, baseSeed, villains, numThreads)` | Same idea with worker threads and distinct seeds. |
| | `exactHuEquityVsRandomHand(heroHoleCards, boardCards)` | **P22** Exact HU equity vs uniform random villain hand; board must have **3–5** cards (full runout enumeration). |
| **Strategy** | `decideAction(state, config, opponentModel?, heroSeat?)` | Rule-based action using MC equity (or strength fallback when sim count is 0), pot odds, call EV. |
| **Pot / chip EV** | `potOddsRatio(pot, toCall)` | `toCall / (pot + toCall)` when valid; else `0`. |
| | `expectedValueCall(equity, pot, toCall)` | Chip EV of calling once vs folding (0); no future streets. |
| | `breakevenCallEquity(potBeforeCall, toCall)` | Same fraction as `potOddsRatio` for chip calls. |
| | `rakeFromPot(potChips, rakeFraction, rakeCap)` | Rake `min(fraction×pot, cap)` for rake-adjusted helpers. |
| | `breakevenCallEquityWithRake(potBeforeCall, toCall, rakeFraction, rakeCap)` | **P9** Breakeven equity when the **final** pot (after call) pays rake under that model. |
| **Stacks & display** | `spr(potChips, effectiveStackChips)` | Stack-to-pot ratio. |
| | `effectiveStack(...stacks)` | Minimum stack; empty → `0`. |
| | `stackInBigBlinds(stackChips, bigBlind)` | Stack size in big blinds. |
| | `potOddsRatioDisplay(potBeforeCall, toCall)` | Display ratio `pot : to_call` (e.g. `3.5` means 3.5:1). |
| | `formatPotOdds(potBeforeCall, toCall, decimals?)` | Human-readable `"x:1"` string. |
| | `harringtonM(stackChips, smallBlind, bigBlind, totalAntes)` | **P11** Harrington **M** = stack / (sb + bb + antes). |
| **Heuristics** | `ruleOfFourEquity(outs)` | Out-count × 4% cap heuristic (turn+river). |
| | `ruleOfTwoEquity(outs)` | Out-count × 2% cap heuristic (one card). |
| | `impliedBreakevenFutureWin(potBeforeCall, toCall, equity)` | Average extra future win needed for a neutral call; `+∞` if equity ≤ 0. |
| | `hypergeometricOneCardHitProbability(outs, unseenCards)` | **P1** One-card draw: `outs / unseenCards`. |
| | `runnerRunnerBackdoorFlushTwoCardProbability(suitCardsRemaining, unseenCards)` | **P3** `C(s,2)/C(u,2)` for two-card runner flush. |
| | `flopToRiverAtLeastOneHitProbability(outs, unseenAfterFlop)` | **P2** Two streets, disjoint out count, no hit on both misses. |
| | `flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop, outsPerCategory[])` | **P4** Sum disjoint categories then same as P2 (caller must ensure categories do not share outs). |
| | `duplicationAdjustedOuts(outs, numVillains, duplicationWeight)` | **P25** Heuristic `outs / (1 + weight × villains)`. |
| **Reverse implied / geometry** | `reverseImpliedOddsMaxFutureLoss(potBeforeCall, toCall, equity)` | **P6** Toy ceiling on extra future loss when losing while keeping the call ≥ 0 EV. |
| | `geometricPotAfterMatchedPotFractions(pot0, fraction, nRounds)` | **P7** Pot after `nRounds` of matched pot-fraction HU betting: `pot0 × (1 + 2f)^n`. |
| **Stats & risk** | `monteCarloStandardError(pHat, nTrials)` | **P15** Binomial SE `√(p̂(1−p̂)/n)`. |
| | `wilsonScoreInterval(successes, nTrials, z)` | **P16** Wilson interval; returns `{ lower, upper }`. |
| | `riskOfRuinDiffusionApprox(driftPerHand, variancePerHand, bankroll)` | **P13** `exp(−2μB/σ²)` style ROR; returns `1` if drift ≤ 0. |
| | `bankrollForTargetRorDiffusion(driftPerHand, variancePerHand, targetRor)` | **P14** Inverse of P13 for bankroll `B`. |
| | `betaBinomialFoldPosterior(priorAlpha, priorBeta, folds, calls)` | **P24** Conjugate Beta update; returns `{ alpha, beta, posteriorMean }`. |
| **Kelly & jam toys** | `kellyCriterionBinary(winProbability, netOdds)` | **P12** Full Kelly `(p·b − (1−p)) / b` for net odds `b`. |
| | `chubukovSymmetricJamBreakevenStack(deadMoneyChips, equity)` | **P23** Toy symmetric jam: `S = equity·dead/(1−2·equity)` for `equity < 0.5`; `+∞` if `equity > 0.5`. |
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
| | `twoStreetPureBluffSameFoldEquity(potBeforeStreet1, betStreet1, betStreet2)` | **P8** Same FE both streets, pure air; may return `NaN`. |
| **Multiway** | `multiwaySymmetricBreakevenCallEquity(potBefore, toCall, symmetricExtraCallers)` | **P5** `k` extra symmetric callers. |
| **ICM** | `icmWinProbabilitiesHarville(stacks[])` | **P17** Harville first-place probabilities. |
| | `icmExpectedPayouts(stacks[], payouts[])` | **P18** Expected payout per seat. |
| | `icmPairwiseBubbleFactor(stacks[], payouts[], heroIndex, villainIndex, potChips)` | **P19** Loss/gain ratio from finite differences on P18. |
| **Side pots** | `sidePotLadderFromCommitments(committedChips[])` | **P20** Main + side layers; each layer `{ potChips, playerCapContribution[] }`. |
| | `layeredPotChipEvFromEquities(layerPotChips[], equityPlayerByLayer[][])` | **P21** Chip EV; each column sums to `1`. |

Supporting types: `NativePokerState`, `NativeBotConfig`, `NativeOpponentModel`, `HandEvalResult`, `DecisionResult`, `WilsonScoreInterval`, `BetaBinomialFoldPosterior`, `SidePotLayer`.

---

## C++ modules

| Module | Headers | Role |
| --- | --- | --- |
| Core chip / odds / probability | [`include/poker/poker_math.hpp`](include/poker/poker_math.hpp) | Pot odds, MDF, fold FE, heuristics, P1–P16 (except ICM), P23 toy jam, P5/P8/P9/P10, rake helpers. |
| ICM | [`include/poker/icm.hpp`](include/poker/icm.hpp) | Harville placement recursion, $EV, bubble factor (**P17–P19**). |
| Side pots | [`include/poker/side_pot.hpp`](include/poker/side_pot.hpp) | Side-pot ladder and layered EV (**P20–P21**). |
| Exact HU | [`include/poker/exact_equity.hpp`](include/poker/exact_equity.hpp) | Enumeration equity vs random hand (**P22**). |

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
