/**
 * TypeScript definitions for `poker-calculations`.
 *
 * Full API reference, examples, and guides: https://poker-calculations.devomb.com
 *
 * @see https://poker-calculations.devomb.com/docs/reference/api
 */

/** Deck id 0..51: `rank * 4 + suit` (rank 0=2 .. 12=A, suit 0=c .. 3=s). */
export type Card52 = number;

/** Hole/board/dead card lists: canonical strings or packed bytes (`Card52` per byte). */
export type CardInput = string[] | Uint8Array;

/** Numeric vector input: `number[]` or `Float64Array`. */
export type F64VectorInput = number[] | Float64Array;

/** ICM / stack math output shape when `returnFormat` is set (default `'array'`). */
export type F64ReturnFormat = 'array' | 'float64';

/** PKST packed game state (`encodePokerState` / `decodePokerState`). */
export type PokerStateBytes = Uint8Array;

/** Optional last argument on `*Async` exports for cooperative cancellation. */
export interface AsyncOptions {
  signal?: AbortSignal;
}

export interface SimBatchSpec {
  holeCards: CardInput;
  board: CardInput;
  numSimulations: number;
  seed: number;
  villains?: number;
}

/** Serialized game state (camelCase) passed to `decideAction`. */
export interface NativePokerState {
  players: Array<{
    name?: string;
    holeCards: CardInput;
    stack?: number;
    committedThisStreet?: number;
    totalCommittedHand?: number;
    folded?: boolean;
    seat?: number;
  }>;
  communityCards: CardInput;
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

/** `format: 'slim'` on `evaluateBestHand` — category ordinal + encoded strength only. */
export interface HandEvalResultSlim {
  rankCategory: number;
  strength: number;
}

export interface HandEvalResult {
  /** Category label (interned at module load). */
  rank: string;
  /** 0..9 — same order as `handRankCategoryOrder` / `highCard` … `royalFlush`. */
  rankCategory: number;
  /** Same bit layout as `evaluateHandStrength` (`pack_hand_strength`). */
  strength: number;
  /** Five encoded kicker values (tie-break order); omitted when `format: 'slim'`. */
  kickers: number[];
}

export type EvaluateBestHandFormat = 'full' | 'slim';

export interface EvaluateBestHandOptions {
  format?: EvaluateBestHandFormat;
}

export type ParseCompactCardListFormat = 'strings' | 'packed';

export interface ParseCompactCardListOptions {
  outFormat?: ParseCompactCardListFormat;
}

/** Optional flags for `compareBestHands`. */
export interface CompareBestHandsOptions {
  /** Skip overlap check between A and B when you guarantee disjoint deck ids. Default false. */
  assumeDisjoint?: boolean;
}

export interface DecisionResult {
  action: 'fold' | 'call' | 'raise' | 'check';
  raiseBy: number;
}

export interface EvaluatorBenchmarkResult {
  legacyEvalsPerSecond: number;
  fastEvalsPerSecond: number;
  implementation: string;
}

/** Sparse villain range: parallel `indices` (two deck ids per combo) and `weights`. */
export interface SparseRangeSpec {
  indices: number[] | Int32Array | Uint32Array;
  weights: F64VectorInput;
}

export interface McEquityDetailedResult {
  estimate: number;
  se: number;
  ciLow: number;
  ciHigh: number;
  n: number;
}

export interface PreflopMatrixOptions {
  iterations?: number;
  seed?: number;
  threads?: number;
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

export interface IcmShapleyValuesOptions {
  method?: 'exact' | 'monteCarlo';
  permutations?: number;
  returnFormat?: F64ReturnFormat;
}

export interface IcmShapleyValuesResult {
  values: number[] | Float64Array;
  method: string;
  se?: number[];
}

export interface IcmFieldPressureIndexResult {
  index: number;
  pairwiseBubbleFactors: number[] | Float64Array;
  argmaxVillain: number;
}

export interface IcmChopParetoPair {
  i: number;
  j: number;
  maxTransfer: number;
}

export interface IcmChopNegotiationAnalysisResult {
  chipChop: number[];
  icm: number[];
  surplus: number[];
  totalPrizePool: number;
  paretoPairs: IcmChopParetoPair[];
}

export interface TournamentDuelAbsorptionResult {
  heroWinProbability: number;
  expectedHands: number;
  heroPrizeEv: number;
}

export interface MaterializedVillainRangeResult {
  weights1326: Float64Array;
  liveComboCount: number;
  weightSum: number;
  shannonEntropy: number;
}

export interface HeroRunoutVulnerabilityResult {
  pNuts: number;
  pDominated: number;
  runoutCount: number;
}

export interface VillainLeapfrogOutCountsResult {
  leapfrogDeckIndices: number[];
  heroImproveDeckIndices: number[];
}

export interface HeroEquityRunoutQuantilesResult {
  mean: number;
  variance: number;
  p05: number;
  p50: number;
  p95: number;
  n: number;
}

export interface CardRemovalGradientResult {
  gradient: Float64Array;
  baseEquity: number;
}

export interface RiverIndifferenceBetResult {
  betSize: number;
  bluffFrequency: number;
  defenderMdf: number;
  evAtIndifference: number;
}

export interface StageMinimaxRegretBetResult {
  bestBet: number;
  minimaxRegret: number;
  evByAction: number[];
}

export interface PushFoldThresholdResult {
  thresholdEquity: number;
  jamEvAtThreshold: number;
}

export interface MultiwayIndependenceGapResult {
  exact: number;
  independentApprox: number;
  gap: number;
  villains: number;
}

export interface SidePotLayerTournamentEvRow {
  chipEv: number;
  icmEvWin: number;
  icmEvLose: number;
  icmMarginal: number;
}

/** N-API addon (220 native function exports): NLHE hand engine, equity (MC + exact), strategy, chip/pot/rake math, ICM, side pots, heuristics, GTO-style frequencies, statistics, tournament/exact-runout/subgame helpers, and related utilities (all implemented in C++). */
export interface PokerCalculations {
  evaluateBestHand(cards: CardInput, options?: EvaluateBestHandOptions): HandEvalResult;
  evaluateBestHand(
    cards: CardInput,
    options: { format: 'slim' }
  ): HandEvalResultSlim;
  /**
   * Encoded strength as `number` (`uint64` bit layout: rank in high bits + five kicker nibbles).
   * Exact integer in IEEE double for sort/compare loops.
   */
  evaluateHandStrength(holeCards: CardInput, board: CardInput): number;
  /**
   * Same encoding as `evaluateHandStrength`, using the in-house stack-only evaluator
   * (`poker-calculations-forge`) used by Monte Carlo and exact enumeration hot paths.
   */
  evaluateHandStrengthFast(holeCards: CardInput, board: CardInput): number;
  /**
   * Benchmark legacy vs forge evaluator throughput on random 7-card spots.
   */
  benchmarkEvaluatorThroughput(iterations?: number): EvaluatorBenchmarkResult;
  /** Same as `benchmarkEvaluatorThroughput`; runs on the libuv thread pool (non-blocking). */
  benchmarkEvaluatorThroughputAsync(
    iterations?: number,
    options?: AsyncOptions
  ): Promise<EvaluatorBenchmarkResult>;
  evaluateHandCategory(holeCards: CardInput, board: CardInput): string;
  /** `true` if `card` parses as a single card (`Ah`, `10c`, …). */
  validateCardString(card: string): boolean;
  /**
   * `true` if any two entries map to the same card. Throws if any entry is invalid.
   * Accepts `string[]` or packed `Uint8Array` (deck ids 0..51).
   */
  cardStringsHaveDuplicate(cards: CardInput): boolean;
  /** Canonical two-character form (`Th`, `Ac`, …); throws if invalid. */
  canonicalCardString(card: string): string;
  /**
   * Parse concatenated or whitespace-separated cards (`AhKh`, `Ah Kh`, `10hKd`); throws on invalid
   * token or duplicate cards. Default `outFormat: 'strings'`; use `'packed'` for `Uint8Array` deck ids.
   */
  parseCompactCardList(text: string, options?: ParseCompactCardListOptions): string[];
  parseCompactCardList(
    text: string,
    options: { outFormat: 'packed' }
  ): Uint8Array;
  /**
   * Compare best 1–7 card lists; returns `-1` / `0` / `1`. Throws on overlap between lists or invalid cards
   * unless `assumeDisjoint: true` (caller guarantees no shared cards between A and B).
   */
  compareBestHands(cardsA: CardInput, cardsB: CardInput, options?: CompareBestHandsOptions): number;
  simulateHandOutcome(
    holeCards: CardInput,
    board: CardInput,
    numSimulations: number,
    seed: number,
    villains?: number
  ): number;
  /** Same as `simulateHandOutcome`; runs on the libuv thread pool (non-blocking). */
  simulateHandOutcomeAsync(
    holeCards: CardInput,
    board: CardInput,
    numSimulations: number,
    seed: number,
    villains?: number,
    options?: AsyncOptions
  ): Promise<number>;
  parallelHandSimulation(
    holeCards: CardInput,
    board: CardInput,
    numSimulations: number,
    baseSeed: number,
    villains: number,
    numThreads: number
  ): number;
  /** Same as `parallelHandSimulation`; runs on the libuv thread pool (non-blocking). */
  parallelHandSimulationAsync(
    holeCards: CardInput,
    board: CardInput,
    numSimulations: number,
    baseSeed: number,
    villains: number,
    numThreads: number,
    options?: AsyncOptions
  ): Promise<number>;
  /** Monte Carlo equity for many spots; returns `Float64Array` (optional preallocated `out`). */
  simulateHandOutcomeBatch(specs: SimBatchSpec[], out?: Float64Array): Float64Array;
  /**
   * Packed batch: `holes` length `2*n`, `boards` length `5*n`, `meta` `Uint32Array` `[numSim, seed, villains]` per row.
   */
  simulateHandOutcomeBatchPacked(
    holes: Uint8Array,
    boards: Uint8Array,
    meta: Uint32Array,
    out?: Float64Array
  ): Float64Array;
  evaluateHandStrengthFastBatch(
    holes: Uint8Array,
    boards: Uint8Array,
    boardCards?: number,
    out?: Float64Array
  ): Float64Array;
  exactHuEquityVsRandomHandBatch(
    holes: Uint8Array,
    boards: Uint8Array,
    boardCards: number,
    out?: Float64Array
  ): Float64Array;
  /** PKST binary encoding of `NativePokerState`. */
  encodePokerState(state: NativePokerState): PokerStateBytes;
  /** Decode PKST bytes to `NativePokerState` (camelCase). */
  decodePokerState(bytes: PokerStateBytes): NativePokerState;
  decideAction(
    state: NativePokerState | PokerStateBytes,
    config: NativeBotConfig,
    opponentModel?: NativeOpponentModel | null,
    heroSeat?: number
  ): DecisionResult;
  /** Same as `decideAction`; runs on the libuv thread pool (non-blocking). */
  decideActionAsync(
    state: NativePokerState | PokerStateBytes,
    config: NativeBotConfig,
    opponentModel?: NativeOpponentModel | null,
    heroSeat?: number,
    options?: AsyncOptions
  ): Promise<DecisionResult>;
  potOddsRatio(pot: number, toCall: number): number;
  /** Chip EV of calling once vs folding (0); same semantics as C++ `expected_value_call`. */
  expectedValueCall(equity: number, pot: number, toCall: number): number;
  /**
   * Chip EV of calling vs folding when the final heads-up pot (after call) pays rake like
   * `breakevenCallEquityWithRake`.
   */
  expectedValueCallWithRake(
    equity: number,
    potBeforeCall: number,
    toCall: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** Same fraction as `potOddsRatio(potBeforeCall, toCall)` for chip calls. */
  breakevenCallEquity(potBeforeCall: number, toCall: number): number;
  spr(potChips: number, effectiveStackChips: number): number;
  effectiveStack(...stacks: number[]): number;
  /** Each stack divided by the sum of stacks (tournament chip share; not Harville ICM). */
  normalizedStackFractions(
    stacks: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  minimumDefenseFrequency(potBeforeOpponentBet: number, opponentBetSize: number): number;
  stackInBigBlinds(stackChips: number, bigBlind: number): number;
  potOddsRatioDisplay(potBeforeCall: number, toCall: number): number;
  formatPotOdds(potBeforeCall: number, toCall: number, decimals?: number): string;
  /**
   * Breakeven call equity from `potOddsRatioDisplay` ratio `R = potBeforeCall / toCall`: `1 / (1 + R)`.
   * `0` when `R` is `+Infinity`.
   */
  breakevenCallEquityFromPotOddsDisplayRatio(displayPotToCallRatio: number): number;
  /**
   * Inverse of `breakevenCallEquityFromPotOddsDisplayRatio`: `(1 - e) / e` for `e` in `(0,1]`; `Infinity`
   * when `e === 0`; `0` when `e === 1`.
   */
  potOddsDisplayRatioFromBreakevenCallEquity(breakevenEquity: number): number;
  /** Reduced integer `pot : to_call` string (e.g. `100`, `50` → `"2:1"`); `toCall === 0` → `∞:1`. */
  formatPotOddsReducedFraction(potBeforeCall: number, toCall: number): string;
  /** Book-style winning odds-against `(1 - equity) / equity`; `Infinity` at `equity === 0`. */
  equityToWinningOddsAgainst(equity: number): number;
  /** `1 / (1 + oddsAgainst)` for `oddsAgainst >= 0`; `0` when `oddsAgainst` is `+Infinity`. */
  winningOddsAgainstToEquity(oddsAgainst: number): number;
  /** NLHE combo count from shorthand (`AA`→6, `AKs`→4, `AKo`→12); throws on invalid notation. */
  preflopCombosFromNotation(notation: string): number;
  /** Sum of `preflopCombosFromNotation` over the list; empty list → `0`. */
  preflopCombosFromNotationsList(notations: string[]): number;
  /**
   * Integer order `0..9` for `evaluateHandCategory` labels (`highCard` … `royalFlush`); throws if unknown.
   */
  handRankCategoryOrder(category: string): number;
  ruleOfFourEquity(outs: number): number;
  ruleOfTwoEquity(outs: number): number;
  /**
   * Algebraic inverse of the uncapped rule-of-two line: `equity * unseen / 2` clamped to `[0, unseen]`
   * (not a full inverse of capped `ruleOfTwoEquity`).
   */
  estimatedOutsFromRuleOfTwo(equity: number, unseenCards: number): number;
  /** Same for two streets vs the rule-of-four line: `equity * unseen / 4` clamped to `[0, unseen]`. */
  estimatedOutsFromRuleOfFour(equity: number, unseenCards: number): number;
  impliedBreakevenFutureWin(potBeforeCall: number, toCall: number, equity: number): number;
  bluffToValueRatio(potBeforeBet: number, betSize: number): number;
  /** `1 / bluffToValueRatio`; `Infinity` when `betSize` is 0. */
  valueToBluffRatio(potBeforeBet: number, betSize: number): number;
  betAsPotFraction(potBeforeBet: number, betSize: number): number;
  /**
   * NL toy: minimum **total** wager after a raise = `currentMaxWager + max(lastRaiseIncrement, bigBlind)`.
   */
  nlMinimumRaiseToTotal(currentMaxWager: number, lastRaiseIncrement: number, bigBlind: number): number;
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
  /** one-card hypergeometric hit rate `outs/unseenCards`. */
  hypergeometricOneCardHitProbability(outs: number, unseenCards: number): number;
  /** runner-runner flush, both cards from suit: C(s,2)/C(u,2). */
  runnerRunnerBackdoorFlushTwoCardProbability(suitCardsRemaining: number, unseenCards: number): number;
  /** flop→river at least one hit from disjoint out count. */
  flopToRiverAtLeastOneHitProbability(outs: number, unseenAfterFlop: number): number;
  /**
   * two out categories with card-level overlap; `sharedAb` = intersection size.
   * Uses union cardinality `outsA + outsB - sharedAb` in the two-draw formula.
   */
  flopToRiverAtLeastOneHitUnionTwoCategories(
    unseenAfterFlop: number,
    outsA: number,
    outsB: number,
    sharedAb: number
  ): number;
  /**
   * three categories; union = `oa+ob+oc - sab - sac - sbc + sabc`.
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
   * four out categories; inclusion–exclusion on **card counts** (pair/triple/four-way intersection sizes).
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
  /** disjoint categories only (must not share outs); sums then same as single-category flop-to-river hit. */
  flopToRiverAtLeastOneHitDisjointOutsSum(unseenAfterFlop: number, outsPerCategory: number[]): number;
  /**
   * structured straight-draw runner model (distinct straight-completing unseen cards).
   * `straightKind`: 0 = gutshot (4 outs), 1 = open-ended (8), 2 = double-belly buster (8).
   * For full-card flop→river P(straight or better) use `straightMadeFlopToRiverExactProbability`.
   */
  runnerRunnerStraightDrawHitProbability(
    straightKind: 0 | 1 | 2,
    deadAmongPatternOuts: number,
    unseenAfterFlop: number
  ): number;
  /** toy reverse-implied ceiling (max future loss when losing). */
  reverseImpliedOddsMaxFutureLoss(potBeforeCall: number, toCall: number, equity: number): number;
  /** pot after `nRounds` of matched pot-fraction betting heads-up. */
  geometricPotAfterMatchedPotFractions(pot0: number, fraction: number, nRounds: number): number;
  /** Harrington M = stack / (sb + bb + antes). */
  harringtonM(stackChips: number, smallBlind: number, bigBlind: number, totalAntes: number): number;
  /**
   * effective M = stack / (sb + bb + antePerActivePlayer * numActivePlayers).
   */
  harringtonMEffective(
    stackChips: number,
    smallBlind: number,
    bigBlind: number,
    antePerActivePlayer: number,
    numActivePlayers: number
  ): number;
  /** effective M with per-seat antes (active seats only); sum of array is total antes in denominator. */
  harringtonMEffectiveActiveAntes(
    stackChips: number,
    smallBlind: number,
    bigBlind: number,
    antesFromActiveSeats: F64VectorInput
  ): number;
  /** Harrington Q: `heroStack / mean(stacks)` (vs average table stack); all stacks must be positive. */
  harringtonQ(heroStack: number, stacks: number[]): number;
  /** One orbit posted cost: `smallBlind + bigBlind + sum(antesFromSeats)`. */
  orbitCostChips(smallBlind: number, bigBlind: number, antesFromSeats: number[]): number;
  /** full Kelly for binary outcome, `netOdds` = net profit per unit staked when you win. */
  kellyCriterionBinary(winProbability: number, netOdds: number): number;
  /** SE of binomial MC estimate. */
  monteCarloStandardError(pHat: number, nTrials: number): number;
  /**
   * Smallest integer `n` so `monteCarloStandardError(pHat, n) <= targetSe` (ceil of `p(1-p)/targetSe²`).
   * Requires `pHat` strictly between `0` and `1`.
   */
  monteCarloTrialsForStandardErrorBound(pHat: number, targetSe: number): number;
  /** Beta–Binomial update on fold frequency. */
  betaBinomialFoldPosterior(
    priorAlpha: number,
    priorBeta: number,
    folds: number,
    calls: number
  ): BetaBinomialFoldPosterior;
  /** heuristic outs discount with multiple villains. */
  duplicationAdjustedOuts(outs: number, numVillains: number, duplicationWeight: number): number;
  /** diffusion-style risk of ruin in (0,1]. */
  riskOfRuinDiffusionApprox(driftPerHand: number, variancePerHand: number, bankroll: number): number;
  /** inverse of `riskOfRuinDiffusionApprox` for bankroll. */
  bankrollForTargetRorDiffusion(
    driftPerHand: number,
    variancePerHand: number,
    targetRor: number
  ): number;
  /** Wilson score interval for a binomial proportion. */
  wilsonScoreInterval(successes: number, nTrials: number, z: number): WilsonScoreInterval;
  /** Agresti–Coull interval; same `{ lower, upper }` shape as Wilson. */
  agrestiCoullInterval(successes: number, nTrials: number, z: number): WilsonScoreInterval;
  /**
   * Normal (Wald) interval `p_hat ± z * SE` clamped to `[0,1]`; weak near `0`/`1` with small `n`.
   */
  normalWaldBinomialInterval(successes: number, nTrials: number, z: number): WilsonScoreInterval;
  /**
   * Hoeffding: smallest integer `n` with `n >= ln(2/delta) / (2*epsilon^2)` for MC proportion error
   * `epsilon` with probability at least `1-delta` (any underlying `p`).
   */
  monteCarloTrialsForHoeffdingBound(epsilon: number, delta: number): number;
  /** Rake model: min(fraction×pot, cap). */
  rakeFromPot(potChips: number, rakeFraction: number, rakeCap: number): number;
  /** breakeven call equity with rake taken from final pot. */
  breakevenCallEquityWithRake(
    potBeforeCall: number,
    toCall: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** semi-bluff FE with rake on `totalPotIfCalled`. */
  breakevenFoldEquitySemiBluffWithRake(
    potBeforeHeroBet: number,
    heroBetSize: number,
    equityWhenCalled: number,
    totalPotIfCalled: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** pure-bluff breakeven FE when fold wins `pot+bet` minus rake on shipped pot. */
  breakevenFoldEquityPureBluffWithRake(
    potBeforeHeroBet: number,
    heroBetOrCallSize: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  /** symmetric extra callers each matching `toCall`. */
  multiwaySymmetricBreakevenCallEquity(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number
  ): number;
  /**
   * same pot geometry with explicit pot-share when hero wins.
   * `shareModel` 0 = winner-take-all; 1 = multiply final pot by `heroFractionWhenWin` (e.g. chop proxy).
   */
  multiwaySymmetricBreakevenCallEquityWithShare(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number,
    shareModel: 0 | 1,
    heroFractionWhenWin: number
  ): number;
  /** same FE both streets, pure air; may return NaN if no root in [0,1]. */
  twoStreetPureBluffSameFoldEquity(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number
  ): number;
  /** EV of two-street pure bluff with independent `fe1`, `fe2`. */
  twoStreetPureBluffEv(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet1: number,
    foldEquityStreet2: number
  ): number;
  /** breakeven second-street FE given first-street FE (may lie outside [0,1]). */
  breakevenFoldEquitySecondStreetPureBluff(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet1: number
  ): number;
  /** breakeven first-street FE given second-street FE (may lie outside [0,1]). */
  breakevenFoldEquityFirstStreetPureBluff(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet2: number
  ): number;
  /** symmetric jam breakeven stack from dead money and equity (toy HU). */
  chubukovSymmetricJamBreakevenStack(deadMoneyChips: number, equity: number): number;
  /** symmetric jam toy EV in chips: `equity * (2 * jamStack + dead) - jamStack`. */
  chubukovSymmetricJamEv(jamStackChips: number, deadMoneyChips: number, equity: number): number;
  /**
   * largest integer jam stack in `[1, maxStackChips]` with nonnegative symmetric-jam EV for the
   * supplied equity (composition with `exactHuEquityVsRandomHand` is left to the caller).
   */
  chubukovMaxSymmetricJamStackChipsBinarySearch(
    equity: number,
    deadMoneyChips: number,
    maxStackChips: number
  ): number;
  /** Harville first-place probabilities. */
  icmWinProbabilitiesHarville(stacks: F64VectorInput, returnFormat?: F64ReturnFormat): number[] | Float64Array;
  /** full Harville placement matrix `[player][finishRank]` (rank 0 = first); flat `n*n` when `returnFormat: 'float64'`. */
  icmHarvillePlacementProbabilities(
    stacks: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[][] | Float64Array;
  /**
   * Per-player probability of finishing in one of the first `k` places (sum of first `k` columns of
   * Harville placement); `k` in `1..stacks.length`.
   */
  icmTopKFinishProbabilities(
    stacks: F64VectorInput,
    k: number,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  /** Harville probability each player finishes last (placement matrix last column). */
  icmLastPlaceProbabilitiesHarville(
    stacks: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  /** ICM expected payouts (prize vector length = players). */
  icmExpectedPayouts(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  /** pairwise bubble factor (finite differences on `icmExpectedPayouts`). */
  icmPairwiseBubbleFactor(
    stacks: number[],
    payouts: number[],
    heroIndex: number,
    villainIndex: number,
    potChips: number
  ): number;
  /** side-pot ladder from per-player committed chips. */
  sidePotLadderFromCommitments(committedChips: F64VectorInput): SidePotLayer[];
  /** chip EV from per-layer pot sizes and per-player per-layer equities (columns sum to 1). */
  layeredPotChipEvFromEquities(
    layerPotChips: F64VectorInput,
    equityPlayerByLayer: number[][] | Float64Array,
    colsOrReturnFormat?: number | F64ReturnFormat,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  /** Sum of `potChips` over side-pot layers. */
  sidePotLayersTotalChips(layers: SidePotLayer[]): number;
  /** Exact HU vs known villain hole cards; board empty (preflop) or 3–5 cards. */
  exactHuEquityVsKnownHand(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput
  ): number;
  /** Exact HU vs weighted villain range (dense `Float64Array(1326)` or sparse spec). */
  exactHuEquityVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** Monte Carlo equity vs weighted villain range. */
  simulateEquityVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    numSimulations: number,
    seed: number
  ): number;
  /** Monte Carlo equity with standard error and Wilson CI. */
  simulateHandOutcomeDetailed(
    holeCards: CardInput,
    board: CardInput,
    numSimulations: number,
    seed: number,
    villains?: number
  ): McEquityDetailedResult;
  /** Preflop 169×169 equity matrix (row-major `Float64Array`, length `169*169`). */
  buildPreflopEquityMatrix(options?: PreflopMatrixOptions): Float64Array;
  /** Change in exact range equity when `removedDeckIndex` is treated as dead. */
  equityDeltaIfCardRemoved(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    removedDeckIndex: number
  ): number;
  /** Independent Weitzman chip-utility ICM (`alpha` defaults to 2). */
  icmExpectedPayoutsWeitzman(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    alpha?: number,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  /** exact HU vs random villain hand; board empty or 3–5 cards. */
  exactHuEquityVsRandomHand(heroHoleCards: CardInput, boardCards: CardInput): number;
  /** Same as `exactHuEquityVsRandomHand`; runs on the libuv thread pool (non-blocking). */
  exactHuEquityVsRandomHandAsync(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    options?: AsyncOptions
  ): Promise<number>;
  /**
   * P(best 7-card hand is straight or straight flush) after two uniformly random **distinct**
   * cards from the remaining deck (unordered two-card subset; same distribution as turn+river multiset).
   * `flopThree` length 3; `knownDead` may be empty.
   */
  straightMadeFlopToRiverExactProbability(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput
  ): number;
  /** Same as `straightMadeFlopToRiverExactProbability`; runs on the libuv thread pool (non-blocking). */
  straightMadeFlopToRiverExactProbabilityAsync(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    options?: AsyncOptions
  ): Promise<number>;
  /**
   * largest integer jam stack in `[1, maxStackChips]` with nonnegative symmetric-jam EV using
   * exact HU equity vs a random hand (`exactHuEquityVsRandomHand`); board 3–5.
   * `maxStackChips` is a double (clamped to int range in native code).
   */
  chubukovMaxSymmetricJamStackBinarySearch(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    deadMoneyChips: number,
    maxStackChips: number
  ): number;
  /**
   * same integer search as `chubukovMaxSymmetricJamStackBinarySearch` (equity from the hand, then
   * `chubukovMaxSymmetricJamStackChipsBinarySearch`). `maxStackChips` is coerced with **int32** semantics in native code.
   */
  chubukovMaxSymmetricJamStackFromHandBinarySearch(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    deadMoneyChips: number,
    maxStackChips: number
  ): number;
  icmShapleyValues(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    options?: IcmShapleyValuesOptions
  ): IcmShapleyValuesResult;
  icmHarvilleStackJacobian(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  icmHarvilleSkillAdjustedPayouts(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    skillWeights: F64VectorInput,
    blend: number,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  icmFieldPressureIndex(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    heroIndex: number,
    potChips: number
  ): IcmFieldPressureIndexResult;
  icmChopNegotiationAnalysis(
    stacks: F64VectorInput,
    payouts: F64VectorInput
  ): IcmChopNegotiationAnalysisResult;
  tournamentDuelAbsorptionProbabilities(
    heroStack: number,
    villainStack: number,
    winProbabilityPerHand: number,
    chipsPerAllIn: number,
    winnerPrize?: number
  ): TournamentDuelAbsorptionResult;
  sidePotLayerTournamentEvDelta(
    tableStacks: F64VectorInput,
    payouts: F64VectorInput,
    heroIndex: number,
    committedChips: F64VectorInput,
    equityPlayerByLayer: number[][] | Float64Array
  ): SidePotLayerTournamentEvRow[];
  materializeVillainRangeAfterBlockers(
    range: Float64Array | SparseRangeSpec,
    heroHoleCards: CardInput,
    boardCards: CardInput,
    knownDead?: CardInput
  ): MaterializedVillainRangeResult;
  bayesianRangeUpdateFromAction(
    range: Float64Array | SparseRangeSpec,
    heroHoleCards: CardInput,
    boardCards: CardInput,
    action: 'fold' | 'call' | 'raise',
    alpha: number
  ): MaterializedVillainRangeResult;
  solveRiverPolarizedIndifferenceBet(
    potBeforeBet: number,
    numValueCombos: number,
    numBluffCombos: number,
    mdf?: number
  ): RiverIndifferenceBetResult;
  solveStageMinimaxRegretBet(
    potBeforeBet: number,
    betSizes: number[],
    villainFoldFreq: number,
    villainCallFreq: number,
    heroEquityWhenCalled: number
  ): StageMinimaxRegretBetResult;
  exactInformationRegretVsClairvoyant(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    potBeforeCall: number,
    toCall: number
  ): number;
  multiwayEquityIndependenceGap(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    numSimulations: number,
    seed: number,
    villains: number
  ): MultiwayIndependenceGapResult;
  solveSymmetricPushFoldThreshold(
    effectiveStack: number,
    smallBlind: number,
    bigBlind: number,
    antePerPlayer: number
  ): PushFoldThresholdResult;
  exactHeroRunoutVulnerability(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    knownDead?: CardInput
  ): HeroRunoutVulnerabilityResult;
  exactHeroRunoutVulnerabilityAsync(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    knownDead?: CardInput,
    options?: AsyncOptions
  ): Promise<HeroRunoutVulnerabilityResult>;
  exactVillainLeapfrogOutCounts(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    knownDead?: CardInput
  ): VillainLeapfrogOutCountsResult;
  exactHeroCategoryJointFlopToRiver(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead?: CardInput
  ): { jointMatrix: Float64Array };
  exactRangeDominatedComboFraction(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  exactHeroEquityRunoutQuantiles(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range?: Float64Array | SparseRangeSpec
  ): HeroEquityRunoutQuantilesResult;
  exactHeroEquityRunoutQuantilesAsync(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range?: Float64Array | SparseRangeSpec,
    options?: AsyncOptions
  ): Promise<HeroEquityRunoutQuantilesResult>;
  exactEquityCardRemovalGradient(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): CardRemovalGradientResult;
  exactEquityCardRemovalGradientAsync(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    options?: AsyncOptions
  ): Promise<CardRemovalGradientResult>;
  /** One-card hypergeometric hit rate `outs / unseen`. */
  flopToTurnAtLeastOneHitProbability(outs: number, unseenAfterFlop: number): number;
  turnToRiverAtLeastOneHitProbability(outs: number, unseenAfterTurn: number): number;
  flopToTurnAtLeastOneHitUnionTwoCategories(
    unseen: number,
    outsA: number,
    outsB: number,
    sharedAb: number
  ): number;
  turnToRiverAtLeastOneHitUnionTwoCategories(
    unseen: number,
    outsA: number,
    outsB: number,
    sharedAb: number
  ): number;
  flopToTurnAtLeastOneHitUnionThreeCategories(
    unseen: number,
    outsA: number,
    outsB: number,
    outsC: number,
    sharedAb: number,
    sharedAc: number,
    sharedBc: number,
    sharedAbc: number
  ): number;
  turnToRiverAtLeastOneHitUnionThreeCategories(
    unseen: number,
    outsA: number,
    outsB: number,
    outsC: number,
    sharedAb: number,
    sharedAc: number,
    sharedBc: number,
    sharedAbc: number
  ): number;
  flopToTurnAtLeastOneHitUnionFourCategories(
    unseen: number,
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
  turnToRiverAtLeastOneHitUnionFourCategories(
    unseen: number,
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
  flopToTurnAtLeastOneHitDisjointOutsSum(unseen: number, outsPerCategory: number[]): number;
  turnToRiverAtLeastOneHitDisjointOutsSum(unseen: number, outsPerCategory: number[]): number;
  hypergeometricTwoCardHitProbability(outs: number, unseenCards: number): number;
  hypergeometricTwoCardMissProbability(outs: number, unseenCards: number): number;
  runnerRunnerBackdoorFlushOneCardProbability(
    suitCardsRemaining: number,
    unseenCards: number
  ): number;
  blockerAdjustedOuts(outs: number, blockerFraction: number): number;
  suitBlockerFraction(suitCardsDead: number, unseen: number): number;
  netPotAfterRake(potChips: number, rakeFraction: number, rakeCap: number): number;
  netPotAfterCallAndRake(
    potBeforeCall: number,
    toCall: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  effectivePotOddsDisplayAfterRake(
    potBeforeCall: number,
    toCall: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  impliedBreakevenTotalPot(potBeforeCall: number, toCall: number, equity: number): number;
  impliedOddsRequiredEquityFromFutureWin(
    potBeforeCall: number,
    toCall: number,
    futureWin: number
  ): number;
  expectedValueRaise(
    equityWhenCalled: number,
    potBeforeRaise: number,
    raiseSize: number,
    foldEquity: number,
    potIfCalled: number
  ): number;
  expectedValueRaiseWithRake(
    equityWhenCalled: number,
    potBeforeRaise: number,
    raiseSize: number,
    foldEquity: number,
    potIfCalled: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  breakevenRaiseEquity(
    potBeforeRaise: number,
    raiseSize: number,
    foldEquity: number,
    potIfCalled: number
  ): number;
  breakevenCallEquityWithPostedAnte(
    potBeforeCall: number,
    toCall: number,
    anteToPost: number
  ): number;
  potSizeAfterHuCall(potBeforeCall: number, toCall: number): number;
  potSizeAfterHuBet(potBeforeBet: number, betSize: number): number;
  expectedValuePerBigBlind(chipEv: number, bigBlind: number): number;
  minimumDefenseFrequencyWithRake(
    potBeforeBet: number,
    betSize: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  alphaFrequencyWithRake(
    potBeforeBet: number,
    betSize: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  bluffToValueRatioWithRake(
    potBeforeBet: number,
    betSize: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  valueToBluffRatioWithRake(
    potBeforeBet: number,
    betSize: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  sprAfterBet(potBeforeBet: number, betSize: number, effectiveStackBeforeBet: number): number;
  sprAfterRaise(potBeforeRaise: number, raiseSize: number, effectiveStackBeforeRaise: number): number;
  commitmentRatioAfterBet(betSize: number, effectiveStackBeforeBet: number): number;
  betSizeToMatchPotFraction(potBeforeBet: number, targetFraction: number): number;
  halfKellyCriterionBinary(winProbability: number, netOdds: number): number;
  quarterKellyCriterionBinary(winProbability: number, netOdds: number): number;
  eighthKellyCriterionBinary(winProbability: number, netOdds: number): number;
  kellyCriterionBinaryClamped(winProbability: number, netOdds: number): number;
  breakevenFoldEquityPureBluffWithAnte(
    potBeforeHeroBet: number,
    heroBetOrCallSize: number,
    anteToPost: number
  ): number;
  breakevenFoldEquitySemiBluffWithAnte(
    potBeforeHeroBet: number,
    heroBetSize: number,
    equityWhenCalled: number,
    totalPotIfCalled: number,
    anteToPost: number
  ): number;
  twoStreetPureBluffEvWithRake(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    foldEquityStreet1: number,
    foldEquityStreet2: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  threeStreetPureBluffSameFoldEquity(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    betStreet3: number
  ): number;
  threeStreetPureBluffEv(
    potBeforeStreet1: number,
    betStreet1: number,
    betStreet2: number,
    betStreet3: number,
    foldEquityStreet1: number,
    foldEquityStreet2: number,
    foldEquityStreet3: number
  ): number;
  multiwaySymmetricBreakevenCallEquityWithRake(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  multiwaySymmetricBreakevenCallEquityWithShareAndRake(
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number,
    shareModel: 0 | 1,
    heroFractionWhenWin: number,
    rakeFraction: number,
    rakeCap: number
  ): number;
  multiwayExpectedValueCall(
    equity: number,
    potBefore: number,
    toCall: number,
    symmetricExtraCallers: number
  ): number;
  reverseImpliedOddsMinEquity(
    potBeforeCall: number,
    toCall: number,
    maxFutureLoss: number
  ): number;
  geometricPotAfterSingleMatchedBet(pot0: number, betSize: number): number;
  binomialProportionCiWidth(successes: number, nTrials: number, z: number): number;
  monteCarloTrialsForWilsonHalfWidth(
    pHat: number,
    targetHalfWidth: number,
    z: number
  ): number;
  varianceToStandardDeviationPerHand(variancePerHand: number): number;
  icmEqualChopPayouts(payouts: F64VectorInput, returnFormat?: F64ReturnFormat): number[] | Float64Array;
  icmChopSurplusVsEqualSplit(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  icmTotalPrizePool(payouts: F64VectorInput): number;
  icmDealEvPerChip(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  icmSatelliteAdvanceProbability(
    stacks: F64VectorInput,
    paidPlaces: number,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  icmPayoutStructureGini(payouts: F64VectorInput): number;
  icmChipLeaderPremiumVsEqualChop(stacks: F64VectorInput, payouts: F64VectorInput): number;
  sidePotLayerCount(committedChips: F64VectorInput): number;
  sidePotBreakevenCallEquity(layerPotChips: number, toCall: number): number;
  preflopCombosFromNotationMinusBlockers(notation: string, deadCardsAmongCombos: number): number;
  stackToPotAfterCall(
    potBeforeCall: number,
    toCall: number,
    effectiveStackBeforeCall: number
  ): number;
  flushMadeFlopToRiverExactProbability(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput
  ): number;
  flushMadeFlopToRiverExactProbabilityAsync(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    options?: AsyncOptions
  ): Promise<number>;
  fullHouseMadeFlopToRiverExactProbability(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput
  ): number;
  fullHouseMadeFlopToRiverExactProbabilityAsync(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    options?: AsyncOptions
  ): Promise<number>;
  tripsMadeFlopToRiverExactProbability(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput
  ): number;
  tripsMadeFlopToRiverExactProbabilityAsync(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    options?: AsyncOptions
  ): Promise<number>;
  twoPairMadeFlopToRiverExactProbability(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput
  ): number;
  twoPairMadeFlopToRiverExactProbabilityAsync(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    options?: AsyncOptions
  ): Promise<number>;
  exactHeroCategoryAtLeastFlopToRiver(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    minCategoryOrder: number
  ): number;
  exactHeroCategoryAtLeastFlopToRiverAsync(
    heroHoleCards: CardInput,
    flopThree: CardInput,
    knownDead: CardInput,
    minCategoryOrder: number,
    options?: AsyncOptions
  ): Promise<number>;
  pushFoldSymmetricEv(equity: number, jamStackChips: number, deadMoneyChips: number): number;
  pushFoldSymmetricBreakevenEquity(jamStackChips: number, deadMoneyChips: number): number;
  openRaiseBreakevenFoldEquity(potBeforeHeroBet: number, heroOpenRaiseSize: number): number;
  callOrFoldChipEvDelta(equity: number, pot: number, toCall: number): number;
  normalizedRangeWeightSum(weights: F64VectorInput): number;
  layeredPotChipEvFromEquitiesWithRake(
    layerPotChips: F64VectorInput,
    equityPlayerByLayer: number[][] | Float64Array,
    rakeFraction: number,
    rakeCap: number,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
  icmExpectedPayoutsDeltaFromChipChop(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;
}

declare const api: PokerCalculations;
export = api;

