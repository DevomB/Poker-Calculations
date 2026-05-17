/** Serialized game state (camelCase) passed to `decideAction`. */
export interface NativePokerState {
  players: Array<{
    name?: string;
    holeCards: string[];
    stack?: number;
    committedThisStreet?: number;
    totalCommittedHand?: number;
    folded?: boolean;
    seat?: number;
  }>;
  communityCards: string[];
  phase: string;
  pot?: number;
  currentBet?: number;
  buttonSeat?: number;
  smallBlind?: number;
  bigBlind?: number;
  actingIndex?: number;
  lastRaiseIncrement?: number;
  streetOpeningIndex?: number;
  actedThisStreet: boolean[];
}

export interface NativeBotConfig {
  aggressionThreshold?: number;
  riskTolerance?: number;
  monteCarloSimulations?: number;
  monteCarloVillains?: number;
  raisePotFraction?: number;
  opponentAggressionWeight?: number;
  rngSeed?: number;
}

export interface NativeOpponentModel {
  aggressionFactor?: number;
  callFrequency?: number;
  foldFrequency?: number;
}

export interface HandEvalResult {
  rank: string;
  kickers: number[];
}

export interface DecisionResult {
  action: 'fold' | 'call' | 'raise' | 'check';
  raiseBy: number;
}

export interface WilsonScoreInterval {
  lower: number;
  upper: number;
}

export interface BetaBinomialFoldPosterior {
  alpha: number;
  beta: number;
  posteriorMean: number;
}

export interface SidePotLayer {
  potChips: number;
  playerCapContribution: number[];
}

/** N-API addon: hand engine, Monte Carlo, strategy, and chip / GTO math (all implemented in C++). */
export interface PokerCalculations {
  evaluateBestHand(cards: string[]): HandEvalResult;
  evaluateHandStrength(holeCards: string[], board: string[]): string;
  evaluateHandCategory(holeCards: string[], board: string[]): string;
  simulateHandOutcome(
    holeCards: string[],
    board: string[],
    numSimulations: number,
    seed: number,
    villains?: number
  ): number;
  parallelHandSimulation(
    holeCards: string[],
    board: string[],
    numSimulations: number,
    baseSeed: number,
    villains: number,
    numThreads: number
  ): number;
  decideAction(
    state: NativePokerState,
    config: NativeBotConfig,
    opponentModel?: NativeOpponentModel | null,
    heroSeat?: number
  ): DecisionResult;
  potOddsRatio(pot: number, toCall: number): number;
  /** Chip EV of calling once vs folding (0); same semantics as C++ `expected_value_call`. */
  expectedValueCall(equity: number, pot: number, toCall: number): number;
  /** Same fraction as `potOddsRatio(potBeforeCall, toCall)` for chip calls. */
  breakevenCallEquity(potBeforeCall: number, toCall: number): number;
  spr(potChips: number, effectiveStackChips: number): number;
  effectiveStack(...stacks: number[]): number;
  minimumDefenseFrequency(potBeforeOpponentBet: number, opponentBetSize: number): number;
  stackInBigBlinds(stackChips: number, bigBlind: number): number;
  potOddsRatioDisplay(potBeforeCall: number, toCall: number): number;
  formatPotOdds(potBeforeCall: number, toCall: number, decimals?: number): string;
  ruleOfFourEquity(outs: number): number;
  ruleOfTwoEquity(outs: number): number;
  impliedBreakevenFutureWin(potBeforeCall: number, toCall: number, equity: number): number;
  bluffToValueRatio(potBeforeBet: number, betSize: number): number;
  /** `1 / bluffToValueRatio`; `Infinity` when `betSize` is 0. */
  valueToBluffRatio(potBeforeBet: number, betSize: number): number;
  betAsPotFraction(potBeforeBet: number, betSize: number): number;
  /**
   * SPR after a call: remaining stack divided by new pot.
   * Assumes heads-up single call: new pot = `potBeforeCall + 2 * toCall`. Throws if `toCall` exceeds stack.
   */
  sprAfterCall(potBeforeCall: number, toCall: number, effectiveStackBeforeCall: number): number;
  commitmentRatio(toCall: number, effectiveStackBeforeCall: number): number;
  /** `1 - minimumDefenseFrequency` = `bet / (pot + bet)`. */
  alphaFrequency(potBeforeBet: number, betSize: number): number;
  breakevenFoldEquityPureBluff(potBeforeHeroBet: number, heroBetOrCallSize: number): number;
  breakevenFoldEquitySemiBluff(
    potBeforeHeroBet: number,
    heroBetSize: number,
    equityWhenCalled: number,
    totalPotIfCalled: number
  ): number;
  /** P1: one-card hypergeometric hit rate `outs/unseenCards`. */
  hypergeometricOneCardHitProbability(outs: number, unseenCards: number): number;
  /** P3: runner-runner flush, both cards from suit: C(s,2)/C(u,2). */
  runnerRunnerBackdoorFlushTwoCardProbability(suitCardsRemaining: number, unseenCards: number): number;
  /** P2: flop→river at least one hit from disjoint out count. */
  flopToRiverAtLeastOneHitProbability(outs: number, unseenAfterFlop: number): number;
  /** P4: sum disjoint out categories then same as P2 (categories must not overlap on cards). */
  flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop: number, outsPerCategory: number[]): number;
  /** P6: toy reverse-implied ceiling (max future loss when losing). */
  reverseImpliedOddsMaxFutureLoss(potBeforeCall: number, toCall: number, equity: number): number;
  /** P7: pot after `nRounds` of matched pot-fraction betting heads-up. */
  geometricPotAfterMatchedPotFractions(pot0: number, fraction: number, nRounds: number): number;
  /** P11: Harrington M = stack / (sb + bb + antes). */
  harringtonM(stackChips: number, smallBlind: number, bigBlind: number, totalAntes: number): number;
  /** P12: full Kelly for binary outcome, `netOdds` = net profit per unit staked when you win. */
  kellyCriterionBinary(winProbability: number, netOdds: number): number;
  /** P15: SE of binomial MC estimate. */
  monteCarloStandardError(pHat: number, nTrials: number): number;
  /** P24: Beta–Binomial update on fold frequency. */
  betaBinomialFoldPosterior(
    priorAlpha: number,
    priorBeta: number,
    folds: number,
    calls: number
  ): BetaBinomialFoldPosterior;
  /** P25: heuristic outs discount with multiple villains. */
  duplicationAdjustedOuts(outs: number, numVillains: number, duplicationWeight: number): number;
  /** P13: diffusion-style risk of ruin in (0,1]. */
  riskOfRuinDiffusionApprox(driftPerHand: number, variancePerHand: number, bankroll: number): number;
  /** P14: inverse of P13 for bankroll. */
  bankrollForTargetRorDiffusion(
    driftPerHand: number,
    variancePerHand: number,
    targetRor: number
  ): number;
  /** P16: Wilson score interval for a binomial proportion. */
  wilsonScoreInterval(successes: number, nTrials: number, z: number): WilsonScoreInterval;
  /** Rake model: min(fraction×pot, cap). */
  rakeFromPot(potChips: number, rakeFraction: number, rakeCap: number): number;
  /** P9: breakeven call equity with rake taken from final pot. */
  breakevenCallEquityWithRake(
    potBeforeCall: number,
    toCall: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** P10: semi-bluff FE with rake on `totalPotIfCalled`. */
  breakevenFoldEquitySemiBluffWithRake(
    potBeforeHeroBet: number,
    heroBetSize: number,
    equityWhenCalled: number,
    totalPotIfCalled: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** P5: symmetric extra callers each matching `toCall`. */
  multiwaySymmetricBreakevenCallEquity(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number
  ): number;
  /** P8: same FE both streets, pure air; may return NaN if no root in [0,1]. */
  twoStreetPureBluffSameFoldEquity(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number
  ): number;
  /** P23: symmetric jam breakeven stack from dead money and equity (toy HU). */
  chubukovSymmetricJamBreakevenStack(deadMoneyChips: number, equity: number): number;
  /** P17: Harville first-place probabilities. */
  icmWinProbabilitiesHarville(stacks: number[]): number[];
  /** P18: ICM expected payouts (prize vector length = players). */
  icmExpectedPayouts(stacks: number[], payouts: number[]): number[];
  /** P19: pairwise bubble factor (finite differences on P18). */
  icmPairwiseBubbleFactor(
    stacks: number[],
    payouts: number[],
    heroIndex: number,
    villainIndex: number,
    potChips: number
  ): number;
  /** P20: side-pot ladder from per-player committed chips. */
  sidePotLadderFromCommitments(committedChips: number[]): SidePotLayer[];
  /** P21: chip EV from per-layer pot sizes and per-player per-layer equities (columns sum to 1). */
  layeredPotChipEvFromEquities(
    layerPotChips: number[],
    equityPlayerByLayer: number[][]
  ): number[];
  /** P22: exact HU vs random villain hand; board must have 3–5 cards. */
  exactHuEquityVsRandomHand(heroHoleCards: string[], boardCards: string[]): number;
}

declare const api: PokerCalculations;
export = api;
