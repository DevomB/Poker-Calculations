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
}

declare const api: PokerCalculations;
export = api;
