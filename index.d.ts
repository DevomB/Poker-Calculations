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
  /**
   * P2: two out categories with card-level overlap; `sharedAb` = intersection size.
   * Uses union cardinality `outsA + outsB - sharedAb` in the two-draw formula.
   */
  flopToRiverAtLeastOneHitUnionTwoCategories(
    unseenAfterFlop: number,
    outsA: number,
    outsB: number,
    sharedAb: number
  ): number;
  /**
   * P2: three categories; union = `oa+ob+oc - sab - sac - sbc + sabc`.
   */
  flopToRiverAtLeastOneHitUnionThreeCategories(
    unseenAfterFlop: number,
    outsA: number,
    outsB: number,
    outsC: number,
    sharedAb: number,
    sharedAc: number,
    sharedBc: number,
    sharedAbc: number
  ): number;
  /**
   * P2: four out categories; inclusion–exclusion on **card counts** (pair/triple/four-way intersection sizes).
   * Pair order (0,1)(0,2)(0,3)(1,2)(1,3)(2,3); triples (0,1,2)(0,1,3)(0,2,3)(1,2,3); last arg four-way.
   */
  flopToRiverAtLeastOneHitUnionFourCategories(
    unseenAfterFlop: number,
    outsA: number,
    outsB: number,
    outsC: number,
    outsD: number,
    s01: number,
    s02: number,
    s03: number,
    s12: number,
    s13: number,
    s23: number,
    s012: number,
    s013: number,
    s023: number,
    s123: number,
    fourWay: number
  ): number;
  /** P2b: disjoint categories only (must not share outs); sums then same as single-count P2. */
  flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop: number, outsPerCategory: number[]): number;
  /**
   * P4 (pattern): structured straight-draw runner model (distinct straight-completing unseen cards).
   * `straightKind`: 0 = gutshot (4 outs), 1 = open-ended (8), 2 = double-belly buster (8).
   * For full-card flop→river P(straight or better) use `straightMadeFlopToRiverExactProbability`.
   */
  runnerRunnerStraightDrawHitProbability(
    straightKind: 0 | 1 | 2,
    deadAmongPatternOuts: number,
    unseenAfterFlop: number
  ): number;
  /** P6: toy reverse-implied ceiling (max future loss when losing). */
  reverseImpliedOddsMaxFutureLoss(potBeforeCall: number, toCall: number, equity: number): number;
  /** P7: pot after `nRounds` of matched pot-fraction betting heads-up. */
  geometricPotAfterMatchedPotFractions(pot0: number, fraction: number, nRounds: number): number;
  /** P11: Harrington M = stack / (sb + bb + antes). */
  harringtonM(stackChips: number, smallBlind: number, bigBlind: number, totalAntes: number): number;
  /**
   * P11: effective M = stack / (sb + bb + antePerActivePlayer * numActivePlayers).
   */
  harringtonMEffective(
    stackChips: number,
    smallBlind: number,
    bigBlind: number,
    antePerActivePlayer: number,
    numActivePlayers: number
  ): number;
  /** P11: effective M with per-seat antes (active seats only); sum of array is total antes in denominator. */
  harringtonMEffectiveActiveAntes(
    stackChips: number,
    smallBlind: number,
    bigBlind: number,
    antesFromActiveSeats: number[]
  ): number;
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
  /** P10 parallel: pure-bluff breakeven FE when fold wins `pot+bet` minus rake on shipped pot. */
  breakevenFoldEquityPureBluffWithRake(
    potBeforeHeroBet: number,
    heroBetOrCallSize: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** P5: symmetric extra callers each matching `toCall`. */
  multiwaySymmetricBreakevenCallEquity(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number
  ): number;
  /**
   * P5: same pot geometry with explicit pot-share when hero wins.
   * `shareModel` 0 = winner-take-all; 1 = multiply final pot by `heroFractionWhenWin` (e.g. chop proxy).
   */
  multiwaySymmetricBreakevenCallEquityWithShare(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number,
    shareModel: 0 | 1,
    heroFractionWhenWin: number
  ): number;
  /** P8: same FE both streets, pure air; may return NaN if no root in [0,1]. */
  twoStreetPureBluffSameFoldEquity(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number
  ): number;
  /** P8: EV of two-street pure bluff with independent `fe1`, `fe2`. */
  twoStreetPureBluffEv(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet1: number,
    foldEquityStreet2: number
  ): number;
  /** P8: breakeven second-street FE given first-street FE (may lie outside [0,1]). */
  breakevenFoldEquitySecondStreetPureBluff(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet1: number
  ): number;
  /** P8: breakeven first-street FE given second-street FE (may lie outside [0,1]). */
  breakevenFoldEquityFirstStreetPureBluff(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet2: number
  ): number;
  /** P23: symmetric jam breakeven stack from dead money and equity (toy HU). */
  chubukovSymmetricJamBreakevenStack(deadMoneyChips: number, equity: number): number;
  /** P23: symmetric jam toy EV in chips: `equity * (2 * jamStack + dead) - jamStack`. */
  chubukovSymmetricJamEv(jamStackChips: number, deadMoneyChips: number, equity: number): number;
  /**
   * P23: largest integer jam stack in `[1, maxStackChips]` with nonnegative symmetric-jam EV for the
   * supplied equity (composition with `exactHuEquityVsRandomHand` is left to the caller).
   */
  chubukovMaxSymmetricJamStackChipsBinarySearch(
    equity: number,
    deadMoneyChips: number,
    maxStackChips: number
  ): number;
  /** P17: Harville first-place probabilities. */
  icmWinProbabilitiesHarville(stacks: number[]): number[];
  /** P17: full Harville placement matrix `[player][finishRank]` (rank 0 = first). */
  icmHarvillePlacementProbabilities(stacks: number[]): number[][];
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
  /**
   * P4 (exact): P(best 7-card hand is straight or straight flush) after two uniformly random **distinct**
   * cards from the remaining deck (unordered two-card subset; same distribution as turn+river multiset).
   * `flopThree` length 3; `knownDead` may be empty.
   */
  straightMadeFlopToRiverExactProbability(
    heroHoleCards: string[],
    flopThree: string[],
    knownDead: string[]
  ): number;
  /**
   * P23: largest integer jam stack in `[1, maxStackChips]` with nonnegative symmetric-jam EV using
   * exact HU equity vs a random hand (`exactHuEquityVsRandomHand`); board 3–5.
   */
  chubukovMaxSymmetricJamStackBinarySearch(
    heroHoleCards: string[],
    boardCards: string[],
    deadMoneyChips: number,
    maxStackChips: number
  ): number;
}

declare const api: PokerCalculations;
export = api;
