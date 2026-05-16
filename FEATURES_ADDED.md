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
| **Strategy** | `decideAction(state, config, opponentModel?, heroSeat?)` | Rule-based action using MC equity (or strength fallback when sim count is 0), pot odds, call EV. |
| **Pot / chip EV** | `potOddsRatio(pot, toCall)` | `toCall / (pot + toCall)` when valid; else `0`. |
| | `expectedValueCall(equity, pot, toCall)` | Chip EV of calling once vs folding (0); no future streets. |
| | `breakevenCallEquity(potBeforeCall, toCall)` | Same fraction as `potOddsRatio` for chip calls. |
| **Stacks & display** | `spr(potChips, effectiveStackChips)` | Stack-to-pot ratio. |
| | `effectiveStack(...stacks)` | Minimum stack; empty → `0`. |
| | `stackInBigBlinds(stackChips, bigBlind)` | Stack size in big blinds. |
| | `potOddsRatioDisplay(potBeforeCall, toCall)` | Display ratio `pot : to_call` (e.g. `3.5` means 3.5:1). |
| | `formatPotOdds(potBeforeCall, toCall, decimals?)` | Human-readable `"x:1"` string. |
| **Heuristics** | `ruleOfFourEquity(outs)` | Out-count × 4% cap heuristic (turn+river). |
| | `ruleOfTwoEquity(outs)` | Out-count × 2% cap heuristic (one card). |
| | `impliedBreakevenFutureWin(potBeforeCall, toCall, equity)` | Average extra future win needed for a neutral call; `+∞` if equity ≤ 0. |
| **GTO-style (toy)** | `minimumDefenseFrequency(potBeforeOpponentBet, opponentBetSize)` | MDF from pot geometry. |
| | `alphaFrequency(potBeforeBet, betSize)` | `1 - MDF` = exploit weight if hero never defends. |
| | `bluffToValueRatio(potBeforeBet, betSize)` | Polarized river combo ratio `bet / (pot + 2×bet)`. |
| | `valueToBluffRatio(potBeforeBet, betSize)` | Reciprocal; `Infinity` when bet is 0. |
| **Sizing & commitment** | `betAsPotFraction(potBeforeBet, betSize)` | Bet as fraction of pot. |
| | `sprAfterCall(potBeforeCall, toCall, effectiveStackBeforeCall)` | SPR after HU single call; throws if `toCall` > stack. |
| | `commitmentRatio(toCall, effectiveStackBeforeCall)` | Fraction of stack put in to call. |
| **Fold equity** | `breakevenFoldEquityPureBluff(potBeforeHeroBet, heroBetOrCallSize)` | FE when equity if called is 0. |
| | `breakevenFoldEquitySemiBluff(potBeforeHeroBet, heroBetSize, equityWhenCalled, totalPotIfCalled)` | Two-outcome model; may exceed 1 if line is −EV even if villain always folds. |

Supporting types: `NativePokerState`, `NativeBotConfig`, `NativeOpponentModel`, `HandEvalResult`, `DecisionResult`.

---

## C++ pure math layer (from `poker_math.hpp`)

The Node addon wraps these `poker::` functions (same semantics as the JS names above):

`pot_odds_ratio`, `expected_value_call`, `spr`, `effective_stack`, `breakeven_call_equity`, `minimum_defense_frequency`, `stack_in_big_blinds`, `pot_odds_ratio_display`, `format_pot_odds`, `rule_of_four_equity`, `rule_of_two_equity`, `implied_breakeven_future_win`, `bluff_to_value_ratio`, `value_to_bluff_ratio`, `bet_as_pot_fraction`, `spr_after_call`, `commitment_ratio`, `alpha_frequency`, `breakeven_fold_equity_pure_bluff`, `breakeven_fold_equity_semi_bluff`.

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
