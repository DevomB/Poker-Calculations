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

/**
 * Sparse PLO range. `packed` is 4 deck ids (0..51) per combo (`length === 4 * n`).
 * Full C(52,4)=270725 is not accepted as a dense vector — pass only live combos.
 * Omitted `weights` default to 1 per combo.
 */
export interface OmahaRangeSpec {
  packed: Uint8Array | number[];
  weights?: F64VectorInput;
}

/** Flop wrap/OESD outs: next cards that make a straight under Omaha 2+3. */
export interface OmahaWrapDrawResult {
  /** Distinct remaining cards that make a straight, straight flush, or royal. */
  outs: number;
  /** Subset of `outs` that is the nut Omaha hand on the 4-card board. */
  nutOuts: number;
}

/**
 * Sparse Big O range. `packed` is 5 deck ids (0..51) per combo (`length === 5 * n`).
 * Omitted `weights` default to 1 per combo.
 */
export interface BigORangeSpec {
  packed: Uint8Array | number[];
  weights?: F64VectorInput;
}

/**
 * 8-or-better low. `ranks` are high-to-low with Ace=1 … 8=8.
 * `key` is packed for comparison (smaller is better). Non-qualifying: `qualifies=false`, `key=0xFFFFFFFF`.
 */
export interface OmahaLoHand {
  qualifies: boolean;
  ranks: number[];
  key: number;
}

export interface OmahaHiLoHands {
  hi: HandEvalResult;
  lo: OmahaLoHand;
}

/**
 * HU PLO-8. `hiEquity` / `loEquity` are side shares (win=1, tie=0.5).
 * No qualifying low → high takes the full pot (`potShare === hiEquity`, `loEquity === 0`).
 * `scoopEquity` = P(win the whole pot). `quarterRate` = P(exactly one side ties).
 */
export interface OmahaHiLoEquity {
  hiEquity: number;
  loEquity: number;
  scoopEquity: number;
  quarterRate: number;
  potShare: number;
}

export interface OmahaHiLoNuttedness {
  hiNuts: boolean;
  loNuts: boolean;
  scoopNuts: boolean;
}

/**
 * Kansas City 2-7 single draw. Ace is **high** (A5432 is Ace-high, not a wheel).
 * Straights and flushes count against you. Best hand is 7-5-4-3-2 rainbow.
 * Packed strength uses the same bit layout as `evaluateHandStrength`; **lower is better**.
 */
export type DeuceSevenCategory =
  | 'nuts'
  | 'smooth'
  | 'rough'
  | 'number'
  | 'paired'
  | 'twoPair'
  | 'trips'
  | 'straight'
  | 'flush'
  | 'fullHouse'
  | 'quads'
  | 'straightFlush';

export interface DeuceSevenDiscardChoice {
  discard: string[];
  keep: string[];
  /** Share of replacements that beat standing pat (ties 0.5). Stand = 0.5. */
  expectedStrength: number;
}

export interface DeuceSevenDrawOptions {
  /** Cards to throw, or a count (best discard of that size). */
  heroDiscard?: number | CardInput;
  villainDiscard?: number | CardInput;
  heroKeep?: CardInput;
  villainKeep?: CardInput;
  extraDead?: CardInput;
  /** Used only when the replacement tree is too large for exact enumeration. Default 400. */
  trials?: number;
  seed?: number;
}

export interface DeuceSevenRoughSmooth {
  /** `-1` if `a` is better 2-7, `1` if `b` is, `0` tie. */
  cmp: number;
  /** Unpaired 8-high whose second card is not a 7. */
  aSmooth: boolean;
  bSmooth: boolean;
}

export interface DeuceSevenDiscardEvTable {
  /** Equity of standing pat vs a standing villain. */
  pat: number;
  /** Best one-card draw equity vs that standing villain. */
  draw1: number;
  draw2: number;
}

/** One-pass HS / PPot / NPot / EHS / EHS2 vs a villain range. `n*` are weighted combo masses. */
export interface HandPotentialBreakdown {
  hs: number;
  ppot: number;
  npot: number;
  ehs: number;
  ehs2: number;
  nBehind: number;
  nAhead: number;
  nTied: number;
}

/** `comboEhsTableVsRange` options. `trials: 0` / omitted = exact next-street enumeration. */
export interface ComboEhsTableOptions {
  trials?: number;
  seed?: number;
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

export interface PkoKnockoutMatrixResult {
  matrix: Float64Array;
  n: number;
}

/** Freezeout ICM plus expected bounty collection (ICMBU). */
export interface PkoIcmbuResult {
  icm: number[] | Float64Array;
  bounty: number[] | Float64Array;
  icmbu: number[] | Float64Array;
}

/** Freezeout vs ICMBU and chip-share of the bounty pool vs expected collection. */
export interface PkoBountyRiskPremiumResult {
  freezeoutIcm: number[];
  icmbu: number[];
  icmbuMinusFreezeout: number[];
  chipShareBountyEv: number[];
  bountyRiskPremium: number[];
}

export interface PkoCallEvResult {
  callEv: number;
  foldEv: number;
  delta: number;
}

export interface PkoJamEvResult {
  jamEv: number;
  foldEv: number;
  delta: number;
}

/**
 * Mystery bounty remaining-prize EV.
 * `oneDraw` is the weighted mean of one knockout. `allRemaining` is the leftover pool
 * (winner-take-all). `sampleK` is E[sum of k draws]; equal-weight WOR is exact via linearity.
 */
export interface MysteryBountyEvResult {
  oneDraw: number;
  allRemaining: number;
  sampleK: number;
  k: number;
}

export interface PkoCoveringHuntResult {
  huntEv: number;
  checkDownEv: number;
  delta: number;
  equityUsed: number;
}

export interface PkoWinnerTakeBountiesResult {
  adjustedPayouts: number[] | Float64Array;
  ev: number[] | Float64Array;
  winProbabilities: number[] | Float64Array;
  bountyToWinnerEv: number[] | Float64Array;
}

/** Equity if called: scalar P(win) with P(tie)=0, `[equity, tie]`, or `[win, tie, lose]`. */
export type PkoOutcomeInput = number | F64VectorInput;

export interface FutureGrowthShareResult {
  netGrowth: number[];
  growthShare: number[];
  survivorCount: number;
}

export interface IcmJamVsFoldEvResult {
  foldEv: number;
  jamEv: number;
  delta: number;
}

export interface IcmCallVsFoldEvResult {
  foldEv: number;
  callEv: number;
  delta: number;
}

export interface IcmStallingEvOptions {
  modelCollision?: boolean;
}

export interface IcmStallingEvResult {
  nowEv: number;
  stallEv: number;
  stallingPremium: number;
  collisionEv: number;
  collisionModeled: boolean;
}

export interface IcmPayJumpSurvivalOptions {
  /** `vanish` (default): busted chips leave the table. `chipLeader`: chips move to the current leader. */
  bustChips?: 'vanish' | 'chipLeader';
}

export interface IcmPayJumpSurvivalResult {
  nowEv: number;
  afterBustEv: number;
  ladderDelta: number;
  bustedIndex: number;
}

export interface IcmDeadPotDollarEvResult {
  nowEv: number;
  winEv: number;
  delta: number;
}

export interface MultiwayWinTieLoseResult {
  win: number[];
  split: number[];
  lose: number[];
}

export interface MultiwaySidePotChipEvResult {
  chipEv: number[];
  layerCount: number;
}

export interface MultiwayAheadFrequencyResult {
  /** 1 if uniquely best on the current flop/turn, `1/k` if tied for best, else 0. */
  pAheadNow: number;
  /** Player 0 exact showdown equity (chop share `1/tiedAtBest`). */
  pWinShowdown: number;
}

export interface MultiwayTieFrequencyResult {
  /** P(player 0 is tied for best at showdown). */
  pHeroSplit: number;
  /** P(two or more players share the best hand at showdown). */
  pAnySplit: number;
}

export interface MultiwayBestWorstRunoutResult {
  supported: boolean;
  bestCard?: string;
  worstCard?: string;
  bestEquity?: number;
  worstEquity?: number;
}

export interface CfrNodeUpdateResult {
  regrets: Float64Array;
  strategy: Float64Array;
}

export interface StrategySupportSizeResult {
  mixedCount: number;
  pureMass: number;
}

export interface CfrRiverSolveResult {
  betFreq: number;
  callFreq: number;
  evBettor: number;
  evDefender: number;
  iterations: number;
  betMix: Float64Array;
  callMix: Float64Array;
}

export interface BestResponseRiverResult {
  value: number;
  callFrequency: number;
  action: 'call' | 'fold';
}

export interface StrategyProfileEvResult {
  evBettor: number;
  evDefender: number;
}

export interface CfrPushFoldResult {
  jamFreq: number;
  callFreq: number;
  evJammer: number;
  evCaller: number;
  iterations: number;
  jamMix: Float64Array;
  callMix: Float64Array;
}

export interface RiverHandClassFreqs {
  airBet: number;
  drawBet: number;
  madeBet: number;
  strongBet: number;
  airCall: number;
  drawCall: number;
  madeCall: number;
  strongCall: number;
}

export interface RiverTopBetCombo {
  comboIndex: number;
  cardA: number;
  cardB: number;
  betFrequency: number;
  weight: number;
}

export interface HuRiverCheckBetTreeResult extends CfrRiverSolveResult {
  classes: RiverHandClassFreqs;
  topBetCombos: RiverTopBetCombo[];
}

/** Vanilla CFR on a HU flop check/bet tree in EHS2-bucket space (K mixed actions). */
export interface FlopBucketCfrResult {
  betFreq: number;
  callFreq: number;
  evBettor: number;
  evDefender: number;
  iterations: number;
  /** Length-K bet frequencies (one info set per bucket). */
  betMix: Float64Array;
  /** Length-K call frequencies. */
  callMix: Float64Array;
}

export interface FlopBucketSolveFromHandsResult extends FlopBucketCfrResult {
  heroBuckets: Int32Array;
  villainBuckets: Int32Array;
  betMix1326: Float64Array;
  callMix1326: Float64Array;
  flopIndex: number;
}

export interface BestResponseFlopBucketsResult {
  value: number;
  callFrequency: number;
  action: 'call' | 'fold';
}

export interface NashPushFoldOptions {
  stackBb?: number;
  heroStack?: number;
  villainStack?: number;
  smallBlind?: number;
  bigBlind?: number;
  /** Total ante already in the pot (chips). */
  ante?: number;
  maxIterations?: number;
  tolerance?: number;
  equityIterations?: number;
  equitySeed?: number;
  otherStacks?: F64VectorInput;
  payouts?: F64VectorInput;
  maxStackBb?: number;
  nOpponents?: number;
  stacks?: F64VectorInput;
  shoverStack?: number;
  callerStacks?: F64VectorInput;
}

/** 81-class 6+ range: notations, notation-weight map, or Float64Array 81 / 169 (2–5 weights must be 0). */
export type ShortDeckRangeInput = string[] | Record<string, number> | Float64Array;

export interface ShortDeckNashOptions {
  stackBb?: number;
  smallBlind?: number;
  bigBlind?: number;
  ante?: number;
  maxIterations?: number;
  tolerance?: number;
  equityIterations?: number;
  equitySeed?: number;
}

export interface NashJamCallSolveResult {
  jam: Float64Array;
  call: Float64Array;
  heroEv: number;
  villainEv: number;
  iterations: number;
}

export interface NashMultiwayShoveCallResult {
  jam: Float64Array;
  calls: Float64Array[];
  iterations: number;
}

/**
 * Joint suit-canonical holes + board. `suitPerm[oldSuit] = newSuit` (0=c … 3=s).
 * Leftover flop symmetry is broken by later streets, then by the sorted holes.
 */
export interface CanonicalHolesAndBoardResult {
  holes: string[];
  board: string[];
  suitPerm: number[];
}

export interface SpinGoNashJamCallResult {
  jam: Float64Array;
  sbCall: Float64Array;
  bbCall: Float64Array;
  iterations: number;
}

export interface LateRegOverlayResult {
  overlayRatio: number;
  registerEv: number;
  icmShare: number;
}

export interface SatelliteTicketEvResult {
  advanceProb: number;
  ticketEv: number;
  chipEvIfDouble: number;
  dollarEvIfDouble: number;
}

export interface SqueezeEvResult {
  squeezeEv: number;
  foldEv: number;
  delta: number;
}

export interface FourBetJamEvResult {
  jamEv: number;
  foldEv: number;
  delta: number;
}

export interface IsoRaiseEvResult {
  isoEv: number;
  checkEv: number;
  foldEv: number;
}

export interface ThreeBetCommitEvResult {
  spr: number;
  stackOff: boolean;
  continueEv: number;
  foldEv: number;
}

/**
 * Heads-up gambler's ruin: each all-in moves `chipsPerAllIn`; hero busts after
 * ceil(heroStack/chipsPerAllIn) net losses and wins after ceil(villainStack/chipsPerAllIn) net wins.
 * Closed form, so `heroWinProbability` is the chip share at p = 0.5.
 */
export interface TournamentDuelAbsorptionResult {
  heroWinProbability: number;
  /** Expected number of all-ins until someone busts. */
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

/**
 * Polarized river bet that makes a bluff-catcher indifferent when the whole range bets:
 * with value share v = value/(value+bluffs), `betSize = (1 − v) × pot / (2v − 1)` (requires
 * v > 0.5; otherwise `betSize` is `Infinity` because every size is a profitable call).
 */
export interface RiverIndifferenceBetResult {
  betSize: number;
  /** Share of the bluff supply needed at `betSize` (1 when the whole range bets). */
  bluffFrequency: number;
  /** `mdf` when supplied, else pot/(pot + betSize). */
  defenderMdf: number;
  /** Villain's bluff-catch EV at `betSize`; 0 at indifference. */
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

export interface RangeComboWeight {
  comboIndex: number;
  cardA: Card52;
  cardB: Card52;
  notation: string;
  weight: number;
}

export interface RangeNotationWeight {
  notation: string;
  weight: number;
}

export interface RangeClassWeights {
  pairs: number;
  suitedBroadways: number;
  offsuitBroadways: number;
  suitedConnectors: number;
  suitedAces: number;
  other: number;
}

export interface BoardTextureResult {
  pairedness: number;
  suitedness: number;
  connectedness: number;
  highCardPressure: number;
  wetness: number;
  staticness: number;
}

export interface CardScore {
  deckIndex: Card52;
  card: string;
  score: number;
}

export interface EquityDistributionResult {
  mean: number;
  variance: number;
  p05: number;
  p50: number;
  p95: number;
  n: number;
}

export interface RangeCoverageResult {
  madeHandShare: number;
  drawShare: number;
  overcardShare: number;
  airShare: number;
}

export interface EvGridRow {
  betSize: number;
  foldFrequency: number;
  equityWhenCalled: number;
  ev: number;
}

export interface EvGridResult {
  rows: EvGridRow[];
  bestBet: number;
  bestEv: number;
}

export interface OpponentBiasResult {
  wentToShowdownRate: number;
  wonAtShowdownRate: number;
  showdownBias: number;
}

export interface LegalActionSummaryResult {
  canFold: boolean;
  canCheck: boolean;
  canCall: boolean;
  canRaise: boolean;
  toCall: number;
  minRaiseTo: number;
  maxRaiseTo: number;
  actingIndex: number;
}

export interface PokerStateValidationResult {
  valid: boolean;
  errors: string[];
}

export interface ActionEvBreakdownResult {
  foldEv: number;
  checkEv: number;
  callEv: number;
  raiseEv: number;
  equity: number;
  toCall: number;
}

export interface DecisionDiagnosticResult {
  decision: DecisionResult;
  legalActions: LegalActionSummaryResult;
  ev?: ActionEvBreakdownResult;
  reason: string;
  error?: string;
}

export interface DecisionFactor {
  name: string;
  weight: number;
  description: string;
}

export interface CandidateAction {
  action: 'fold' | 'check' | 'call' | 'raise';
  amount: number;
}

/** N-API addon (370 native function exports): NLHE hand engine, equity (MC + exact), strategy, chip/pot/rake math, ICM, side pots, heuristics, GTO-style frequencies, statistics, tournament/exact-runout/subgame helpers, board texture, suit isomorphism, range tools, opponent modeling, PKO/FGS/Nash/CFR/exact-multiway, Omaha Hi, PLO-8, Big O, MTT spots, short deck (6+), hand potential (HS/PPot/NPot/EHS), 2-7 single draw, 7-card stud / razz, bucketed flop CFR, and related utilities (all implemented in C++). */
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
  /**
   * Chip EV of calling once vs folding (0): `equity × pot − (1 − equity) × toCall`, where `pot`
   * is everything in the middle before hero's call (villain's bet included). Zero exactly at
   * `breakevenCallEquity(pot, toCall)`.
   */
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
  /**
   * Hand strength vs a villain range on this flop/turn: P(ahead) + 0.5 P(tie) if the hand
   * ended now. Ties follow the HU chop (half). Blocked combos are removed.
   */
  handStrengthVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** One-card PPot (flop→turn or turn→river). `P(behind now and ahead later) / P(behind now)` with half-chop ties. */
  positivePotentialVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** One-card NPot. `P(ahead now and behind later) / P(ahead now)` with half-chop ties. */
  negativePotentialVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** EHS = HS × (1 − NPot) + (1 − HS) × PPot. */
  effectiveHandStrength(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** Billings' two-opponent form: EHS2 = HS² × (1 − NPot) + (1 − HS²) × PPot. */
  effectiveHandStrengthSquared(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** HS, PPot, NPot, EHS, EHS2, and weighted ahead/tied/behind masses in one pass. */
  handPotentialBreakdown(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    options?: { streets?: 1 | 2 }
  ): HandPotentialBreakdown;
  /** Flop→river (two-card) PPot. Board must be 3 cards. */
  twoStreetPositivePotential(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** Flop→river (two-card) NPot. Board must be 3 cards. */
  twoStreetNegativePotential(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec
  ): number;
  /** Map EHS in `[0, 1]` to `k` equal-width buckets `[0, k)`. */
  equityBucketFromEhs(ehs: number, k: number): number;
  /**
   * EHS for all 1326 hero combos vs `range` on this flop/turn (`0` if blocked by the board).
   * Exact when `trials` is omitted/0. Cost is combos × range × next cards and is synchronous:
   * a full 1326-combo range takes minutes on either street (the turn is slower per evaluation).
   * Pass `{ trials, seed }` to Monte Carlo the next card, or use a small range.
   */
  comboEhsTableVsRange(
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    options?: ComboEhsTableOptions
  ): Float64Array;
  /**
   * EHS2 bucket id for every 1326 hero combo vs `range` on this flop/turn.
   * Live combos are in `[0, k)`; board-blocked combos are `-1`.
   * Default `k` is `flopBucketCountDefault()` (20). Pass `{ trials, seed }` to Monte Carlo
   * next-street cards on a flop (same cost note as `comboEhsTableVsRange`).
   */
  ehs2BucketsVsRange(
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    k?: number,
    options?: ComboEhsTableOptions
  ): Int32Array;
  /** Normalize a 1326 range into `k` bucket masses (sum ≈ 1). */
  bucketMassFromRange(
    range: Float64Array | SparseRangeSpec,
    buckets: Int32Array | number[],
    k?: number
  ): Float64Array;
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
  materializeVillainRangeAfterBlockers(
    range: Float64Array | SparseRangeSpec,
    heroHoleCards: CardInput,
    boardCards: CardInput,
    knownDead?: CardInput
  ): MaterializedVillainRangeResult;
  solveRiverPolarizedIndifferenceBet(
    potBeforeBet: number,
    numValueCombos: number,
    numBluffCombos: number,
    mdf?: number
  ): RiverIndifferenceBetResult;
  exactInformationRegretVsClairvoyant(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: Float64Array | SparseRangeSpec,
    potBeforeCall: number,
    toCall: number
  ): number;
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
  ): {
    /** 9×9 row-major `[turnCategory * 9 + riverCategory]`; royal flush is folded into straight flush (8). */
    jointMatrix: Float64Array;
  };
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
  /** `potBeforeCall + toCall`: the pot already holds villain's bet, so only hero's call is added. */
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
  /** Combos of `notation` ("AA", "AKs", "AKo") whose two cards are both outside `deadCards`. */
  preflopCombosFromNotationMinusBlockers(notation: string, deadCards: CardInput): number;
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

  normalizeSparseRange(range: SparseRangeSpec | Float64Array): Float64Array;
  pruneRangeByMinWeight(range: SparseRangeSpec | Float64Array, minWeight: number): SparseRangeSpec;
  mergeSparseRanges(
    a: SparseRangeSpec | Float64Array,
    b: SparseRangeSpec | Float64Array,
    weightA: number,
    weightB: number
  ): Float64Array;
  intersectSparseRanges(a: SparseRangeSpec | Float64Array, b: SparseRangeSpec | Float64Array): Float64Array;
  subtractSparseRange(base: SparseRangeSpec | Float64Array, remove: SparseRangeSpec | Float64Array): Float64Array;
  rangeComboCount(range: SparseRangeSpec | Float64Array, minWeight?: number): number;
  rangeShannonEntropy(range: SparseRangeSpec | Float64Array): number;
  rangeGiniCoefficient(range: SparseRangeSpec | Float64Array): number;
  rangeCoverageFraction(range: SparseRangeSpec | Float64Array): number;
  rangeWeightTopKMass(range: SparseRangeSpec | Float64Array, k: number): number;
  rangeDistanceL1(a: SparseRangeSpec | Float64Array, b: SparseRangeSpec | Float64Array): number;
  rangeDistanceL2(a: SparseRangeSpec | Float64Array, b: SparseRangeSpec | Float64Array): number;
  rangeDistanceJensenShannon(a: SparseRangeSpec | Float64Array, b: SparseRangeSpec | Float64Array): number;
  rangeCosineSimilarity(a: SparseRangeSpec | Float64Array, b: SparseRangeSpec | Float64Array): number;
  rangeTopCombos(range: SparseRangeSpec | Float64Array, k: number): RangeComboWeight[];
  rangeBucketWeightsByHandClass(range: SparseRangeSpec | Float64Array): RangeClassWeights;
  rangeBucketWeightsByNotation(range: SparseRangeSpec | Float64Array): RangeNotationWeight[];
  rangeFromNotationWeights(entries: RangeNotationWeight[]): Float64Array;
  rangeBlockerPressureByCard(range: SparseRangeSpec | Float64Array, deadCards?: CardInput): Float64Array;
  rangeRemovalSensitivityVsHero(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: SparseRangeSpec | Float64Array
  ): Float64Array;

  blockerMatrixByCard(range: SparseRangeSpec | Float64Array, board?: CardInput): Float64Array;

  geometricStreetSizingPlan(pot: number, effectiveStack: number, streetsRemaining: number): Float64Array;
  thinValueMargin(heroEquityWhenCalled: number, pot: number, betSize: number): number;
  betSizingIndifferencePoint(pot: number, foldFrequency: number, equityWhenCalled: number): number;
  opponentFoldToCbetPosterior(
    priorAlpha: number,
    priorBeta: number,
    folds: number,
    continues: number
  ): BetaBinomialFoldPosterior;
  opponentAggressionFactor(bets: number, raises: number, calls: number): number;
  opponentRangeElasticityFromSizing(sizes: F64VectorInput, continueRates: F64VectorInput): number;
  villainPolarizedRangeScore(range: SparseRangeSpec | Float64Array, board: CardInput): number;
  legalActionSummary(state: NativePokerState | PokerStateBytes): LegalActionSummaryResult;
  actionMaskFromState(state: NativePokerState | PokerStateBytes): number;
  normalizeBotConfig(config: Partial<NativeBotConfig>): NativeBotConfig;
  validatePokerState(state: NativePokerState | PokerStateBytes): PokerStateValidationResult;
  stateToFeatureVector(state: NativePokerState | PokerStateBytes): Float64Array;
  decideActionWithDiagnostics(
    state: NativePokerState | PokerStateBytes,
    config: NativeBotConfig,
    opponentModel?: NativeOpponentModel | null,
    heroSeat?: number
  ): DecisionDiagnosticResult;
  runBotPolicyBatch(
    states: Array<NativePokerState | PokerStateBytes>,
    config: NativeBotConfig,
    opponentModels?: Array<NativeOpponentModel | null>
  ): DecisionDiagnosticResult[];
  /**
   * PKO knockout matrix, entry `i*n+j` = P(i collects j's bounty). P(j busts) = 1 − stack_j/total
   * (Harville first place). Given a bust, collectors are weighted by stack_i × stack_i/(stack_i+stack_j)
   * over the players covering j (everyone else when nobody covers). Columns sum to P(j busts), so
   * every bounty except the winner's own is paid out. Diagonal 0. Flat n×n row-major `Float64Array` plus `n`.
   */
  pkoKnockoutProbabilityMatrix(stacks: F64VectorInput): PkoKnockoutMatrixResult;

  /**
   * E[bounty $] per player: sum_j bountyValue[j] * P(i knocks j). Self-bounty ignored.
   */
  pkoExpectedBountyCollection(
    stacks: F64VectorInput,
    bountyValues: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;

  /**
   * ICMBU: freezeout `icmExpectedPayouts` plus `pkoExpectedBountyCollection`.
   * Zero bounties match freezeout ICM. Zero-stack seats get last-place prizes.
   */
  pkoIcmbuPayouts(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    bountyValues: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): PkoIcmbuResult;

  /**
   * Freezeout ICM vs ICMBU, and chip-EV share of the bounty pool vs expected collection
   * (`bountyRiskPremium` = chip share − expected collection).
   */
  pkoBountyRiskPremium(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    bountyValues: F64VectorInput
  ): PkoBountyRiskPremiumResult;

  /**
   * $EV(call all-in) vs $EV(fold) when villain has a bounty. Fold leaves stacks as given
   * (pot already posted). Call uses ICM on post-hand stacks; hero collects villain's bounty
   * if the villain busts.
   */
  pkoCallEvVsShove(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    bountyValues: F64VectorInput,
    heroIndex: number,
    villainIndex: number,
    pot: number,
    heroEquityIfCall: PkoOutcomeInput
  ): PkoCallEvResult;

  /**
   * $EV(jam) vs $EV(fold) including bounties. Villain folds with `foldEquity`: hero is assigned
   * `pot`. Called: same all-in resolution as `pkoCallEvVsShove`.
   */
  pkoJamEvVsFold(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    bountyValues: F64VectorInput,
    heroIndex: number,
    villainIndex: number,
    pot: number,
    foldEquity: number,
    equityWhenCalled: PkoOutcomeInput
  ): PkoJamEvResult;

  /**
   * Remaining mystery prizes: weighted mean of one knockout, leftover-pool sum, and optional
   * E[sum of k draws] (`k` omitted or 0 skips the sample). Equal weights if `weights` omitted.
   */
  mysteryBountyExpectedValue(
    values: F64VectorInput,
    weights?: F64VectorInput | number,
    k?: number
  ): MysteryBountyEvResult;

  /**
   * Progressive KO posted bounty: `base + carryFraction * collected`.
   * `knockouts` length n = dollars already collected onto each head; n×n (nested or flat) =
   * M[i][j] weight that i collected j's *base* bounty. `carryFraction` 1.0 = classic PKO.
   */
  progressiveKoPostedBounty(
    baseBounties: F64VectorInput,
    knockouts: F64VectorInput | number[][],
    carryFraction: number,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;

  /**
   * Isolate vs a covered shorter stack (HU all-in, rest fold). Default equity is the
   * stack-ratio proxy hunter / (hunter + prey). Check-down is the fold $EV.
   */
  pkoCoveringHuntEv(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    bountyValues: F64VectorInput,
    hunterIndex: number,
    preyIndex: number,
    pot: number,
    equity?: number
  ): PkoCoveringHuntResult;

  /**
   * Add leftover bounty pool to first prize. Returns adjusted payouts, ICM $EV on those
   * payouts, Harville win probs, and P(i first) * remaining pool.
   */
  pkoWinnerTakeRemainingBounties(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    remainingBountyPool: number,
    returnFormat?: F64ReturnFormat
  ): PkoWinnerTakeBountiesResult;

  /**
   * Hero posts `heroPost`; every other seat posts `posts[i]`. Subtract (clamp 0), then ICM
   * on remaining stacks. Posted chips are dead for placement.
   */
  icmPayoutsAfterBlindPost(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    heroIndex: number,
    heroPost: number,
    posts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;

  /**
   * Calling bubble factor for one hero-vs-villain all-in:
   * `(EV_now - EV_lose) / (EV_win - EV_now)` after transferring `chipsAtRisk`.
   * Distinct from `icmPairwiseBubbleFactor`. Tiny gain with a real loss returns `+Infinity`.
   */
  icmCallingBubbleFactor(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    heroIndex: number,
    villainIndex: number,
    chipsAtRisk: number
  ): number;

  /** FGS across a blind schedule. Each level applies `orbitsAtLevel[i]` average-position orbits. */
  fgsPayoutsBlindSchedule(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    smallBlinds: F64VectorInput,
    bigBlinds: F64VectorInput,
    antes: F64VectorInput,
    orbitsAtLevel: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;

  /**
   * $EV of hero winning a dead pot of `deadChips` (chips in the middle owned by nobody).
   * Two-point ICM: hero stack += deadChips vs ICM now.
   */
  icmDeadPotDollarEv(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    heroIndex: number,
    deadChips: number
  ): IcmDeadPotDollarEvResult;

  /**
   * Exact 3-way equity for known holes. Board 0–5 cards; optional dead/muck.
   * Equities length 3, sum to 1. Ties split `1/tiedAtBest`.
   */
  exactThreeWayEquityKnownHands(
    hand0: CardInput,
    hand1: CardInput,
    hand2: CardInput,
    boardCards: CardInput,
    deadCards?: CardInput
  ): number[];

  /**
   * Per-player win / split / lose frequencies (win = unique best, split = tied best, lose = rest).
   */
  exactThreeWayWinTieLoseKnownHands(
    hand0: CardInput,
    hand1: CardInput,
    hand2: CardInput,
    boardCards: CardInput,
    deadCards?: CardInput
  ): MultiwayWinTieLoseResult;

  /** Exact 4-way equity for known holes. Equities length 4, sum to 1. */
  exactFourWayEquityKnownHands(
    hand0: CardInput,
    hand1: CardInput,
    hand2: CardInput,
    hand3: CardInput,
    boardCards: CardInput,
    deadCards?: CardInput
  ): number[];

  /**
   * Exact n-way equity for 3–6 known hole pairs (n<3 is rejected; use HU APIs).
   * Enumerates remaining boards. Optional dead/muck.
   */
  exactMultiwayEquityKnownHands(
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards?: CardInput
  ): number[];

  /** Exact win / split / lose frequencies for 2..9 known hands (general form of the 3-way export). */
  exactMultiwayWinTieLoseKnownHands(
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards?: CardInput
  ): MultiwayWinTieLoseResult;

  /** Same as `exactMultiwayEquityKnownHands` with required dead/muck cards. */
  exactMultiwayEquityWithDeadCards(
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards: CardInput
  ): number[];

  /**
   * Side-pot chip EV from commitments and exact equities among players eligible for each layer.
   */
  exactMultiwaySidePotChipEv(
    committedChips: F64VectorInput,
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards?: CardInput
  ): MultiwaySidePotChipEvResult;

  /** P(showdown split involving hero) and P(any split). */
  exactMultiwayTieFrequency(
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards?: CardInput
  ): MultiwayTieFrequencyResult;

  /** Number of remaining boards given holes + board + optional dead. */
  exactMultiwayRunoutCount(
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards?: CardInput
  ): number;

  /**
   * Best and worst next-street card for hero (player 0) by exact equity after that card.
   * Turn card when board has 3; river card when board has 4. Otherwise `{ supported: false }`.
   */
  exactMultiwayBestWorstRunout(
    holeHands: CardInput[],
    boardCards: CardInput,
    deadCards?: CardInput
  ): MultiwayBestWorstRunoutResult;

  /**
   * Regret matching: `max(r,0)/sum`. Uniform if every regret is ≤ 0.
   */
  regretMatchingStrategy(regrets: F64VectorInput): Float64Array;

  /**
   * HU river CFR. Tree: bettor Check or Bet; vs Check defender checks back to showdown;
   * vs Bet defender Fold or Call. Showdown via exact 7-card compare.
   * `heroRange` is the bettor. Default 400 iterations.
   */
  cfrRiverBetCallFoldSolve(
    pot: number,
    betSize: number,
    heroRange: SparseRangeSpec | Float64Array,
    villainRange: SparseRangeSpec | Float64Array,
    board: CardInput,
    iterations?: number
  ): CfrRiverSolveResult;

  /**
   * Hero is the defender. `villainBetMix` is a scalar bet frequency or a length-1326 mix.
   */
  bestResponseRiver(
    pot: number,
    betSize: number,
    heroRange: SparseRangeSpec | Float64Array,
    villainRange: SparseRangeSpec | Float64Array,
    board: CardInput,
    villainBetMix: number | F64VectorInput
  ): BestResponseRiverResult;

  /**
   * NashConv = `0.5 * (BR0 + BR1 - EV0 - EV1)` on the river check/bet tree. ≥ 0.
   */
  exploitabilityRiver(
    pot: number,
    betSize: number,
    heroRange: SparseRangeSpec | Float64Array,
    villainRange: SparseRangeSpec | Float64Array,
    board: CardInput,
    bettorMix: number | F64VectorInput,
    callerMix: number | F64VectorInput
  ): number;

  /**
   * Fictitious play on the same river check/bet tree as `cfrRiverBetCallFoldSolve`.
   */
  fictitiousPlayRiver(
    pot: number,
    betSize: number,
    heroRange: SparseRangeSpec | Float64Array,
    villainRange: SparseRangeSpec | Float64Array,
    board: CardInput,
    iterations?: number
  ): CfrRiverSolveResult;

  /**
   * Chip EV of a fixed bet/call profile. No solving.
   */
  evOfStrategyProfile(
    pot: number,
    betSize: number,
    heroRange: SparseRangeSpec | Float64Array,
    villainRange: SparseRangeSpec | Float64Array,
    board: CardInput,
    bettorMix: number | F64VectorInput,
    callerMix: number | F64VectorInput
  ): StrategyProfileEvResult;

  /**
   * `mixedCount` = action probs in `(eps, 1-eps)`. `pureMass` = mean of entries `≥ 1-eps`.
   */
  strategySupportSize(actionProbs: F64VectorInput, eps?: number): StrategySupportSizeResult;

  /**
   * One CFR info-set step: `regret += reach * instantaneous`, then regret-match.
   */
  cfrNodeReachUpdate(
    cumulativeRegrets: F64VectorInput,
    instantaneousRegrets: F64VectorInput,
    reach?: number
  ): CfrNodeUpdateResult;

  /**
   * Full river check/bet CFR plus air/draw/made/strong frequencies and top-k bet combos.
   */
  solveHuRiverCheckBetTree(
    pot: number,
    betSize: number,
    heroRange: SparseRangeSpec | Float64Array,
    villainRange: SparseRangeSpec | Float64Array,
    board: CardInput,
    iterations?: number,
    topK?: number
  ): HuRiverCheckBetTreeResult;

  /** Copy each bucket's mixed action onto every 1326 combo in that bucket. */
  flopBucketStrategyTo1326(
    bucketMix: F64VectorInput,
    comboBuckets: Int32Array | number[]
  ): Float64Array;

  /**
   * Cache key: canonical flop index (`0..countCanonicalFlops()-1`) plus millichip
   * pot/stack. Suit-isomorphic flops share the same key.
   */
  canonicalFlopCfrKey(flop: CardInput, pot: number, stack?: number): string;

  /** Default EHS2 bucket count (20). */
  flopBucketCountDefault(): number;

  /**
   * HU Nash jam frequencies (169). SB jam/fold vs BB call/fold, chip EV.
   * Accepts `stackBb` or `{ stackBb, smallBlind, bigBlind, ante, ... }`.
   */
  nashHeadsUpJamRange(
    stackBbOrOptions: number | NashPushFoldOptions,
    smallBlind?: number,
    bigBlind?: number,
    ante?: number
  ): Float64Array;

  /** HU Nash call frequencies vs jam (169), same order and inputs as `nashHeadsUpJamRange`. */
  nashHeadsUpCallRange(
    stackBbOrOptions: number | NashPushFoldOptions,
    smallBlind?: number,
    bigBlind?: number,
    ante?: number
  ): Float64Array;
  nashHeadsUpJamCallSolve(
    stackBbOrOptions: number | NashPushFoldOptions,
    smallBlind?: number,
    bigBlind?: number,
    ante?: number
  ): NashJamCallSolveResult;

  /**
   * SB vs BB jam/fold (SB blind is dead in the pot). Same solve as HU Nash;
   * complete-or-jam is not modeled.
   */
  nashBlindVsBlindSolve(
    stackBbOrOptions: number | NashPushFoldOptions,
    smallBlind?: number,
    bigBlind?: number,
    ante?: number
  ): NashJamCallSolveResult;

  /** Per-hand max stack in BB that still jams at Nash (indifference / threshold). */
  nashJamFoldChart169(
    bigBlindOrOptions: number | NashPushFoldOptions,
    ante?: number,
    stackBb?: number
  ): Float64Array;

  /** Per-hand max stack in BB that still calls a jam at Nash. */
  nashCallChart169(
    bigBlindOrOptions: number | NashPushFoldOptions,
    ante?: number,
    stackBb?: number
  ): Float64Array;

  /** Stack in BB where jam EV ≈ fold EV vs a Nash caller for one hand (`"AKo"` or 0..168). */
  nashIndifferenceStackBb(
    hand: string | number,
    maxStackBbOrOptions?: number | NashPushFoldOptions,
    smallBlind?: number,
    bigBlind?: number,
    ante?: number
  ): number;

  /**
   * HU Nash with Harville ICM $EV. `otherStacks` are remaining table stacks;
   * `payouts` is first-to-last prize. Showdown ties use equity split (no chop vector).
   */
  nashIcmHeadsUpJamCallSolve(options: NashPushFoldOptions): NashJamCallSolveResult;

  /**
   * Suit-canonical 3-card flop. Rainbow / two-tone / monotone collapse under S4.
   * Returns 3 canonical card strings, sorted by deck id (`rank*4+suit`).
   */
  canonicalFlopBoard(flop: CardInput): string[];

  /**
   * Canonical 3–5 card board. First 3 cards are the flop (set); 4th is turn; 5th is river.
   * Re-solves the 24 suit perms on the street tuple (flop iso first; turn/river break leftover
   * symmetry). Not a board-texture score.
   */
  canonicalBoard(board: CardInput): string[];

  /**
   * Remap hero holes + board by one suit permutation. `suitPerm` is the map to apply to ranges.
   */
  canonicalHolesAndBoard(holes: CardInput, board: CardInput): CanonicalHolesAndBoardResult;

  /** Length-4 perm: `perm[oldSuit] = newSuit` (0=c, 1=d, 2=h, 3=s). Flop-only; leftover suits lex-smallest. */
  suitPermFromCanonicalFlop(flop: CardInput): number[];

  /** Apply a 4-suit perm to any card list; input order is preserved. */
  applySuitPermToCards(cards: CardInput, perm: number[]): string[];

  /** Permute a dense 1326 range through the suit map. Total mass is preserved. */
  applySuitPermToRange1326(range: Float64Array, perm: number[]): Float64Array;

  /**
   * Orbit size: how many raw unordered flops map to this class (1–24).
   * Unpaired rainbow=24, two-tone=12, monotone=4. Pair/trips shrink further (12 or 4).
   */
  isomorphicFlopOrbitSize(flop: CardInput): number;

  /** Always 1755. */
  countCanonicalFlops(): number;

  /** Stable index 0..1754 of the canonical flop (sorted packed deck-id order). */
  isomorphicFlopIndex(flop: CardInput): number;

  /** Inverse of `isomorphicFlopIndex`. */
  flopIndexToCanonical(index: number): string[];

  /**
   * Best 5-card Omaha Hi hand: exactly 2 hole + exactly 3 board.
   * `holeCards` must be 4 cards; `boardCards` must be 3–5 (flop C(4,2)=6, river C(4,2)*C(5,3)=60).
   * Same `HandEvalResult` shape as `evaluateBestHand`.
   */
  evaluateOmahaBestHand(
    holeCards: CardInput,
    boardCards: CardInput,
    options?: EvaluateBestHandOptions
  ): HandEvalResult;
  evaluateOmahaBestHand(
    holeCards: CardInput,
    boardCards: CardInput,
    options: { format: 'slim' }
  ): HandEvalResultSlim;

  /**
   * Omaha Hi strength using the same `pack_hand_strength` encoding as `evaluateHandStrength`
   * (5-card rank + kickers). Not a 7-card best-of-9.
   */
  evaluateOmahaHandStrength(holeCards: CardInput, boardCards: CardInput): number;

  /**
   * Exact HU Omaha Hi equity vs a known 4-card hand. Board 0–5; remaining runouts enumerated.
   * Ties count as 0.5.
   */
  exactHuOmahaEquityVsKnown(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput
  ): number;

  /** Monte Carlo Omaha Hi equity vs a uniform random 4-card villain. */
  simulateOmahaEquityVsRandom(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number;

  /** Monte Carlo Omaha Hi equity vs a sparse weighted 4-card range (`OmahaRangeSpec`). */
  simulateOmahaEquityVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: OmahaRangeSpec,
    trials: number,
    seed: number
  ): number;

  /** Remaining 4-card combo count: `C(52 − |unique dead|, 4)`. Throws on duplicate dead cards. */
  omahaComboCount(deadCards: CardInput): number;

  /**
   * True iff hero’s Omaha Hi hand is unbeaten by every other 4-card combo that avoids
   * `boardCards` and optional `extraDead`.
   */
  omahaNutsOnBoard(heroHoleCards: CardInput, boardCards: CardInput, extraDead?: CardInput): boolean;

  /**
   * Flop wrap/OESD outs. Counts remaining cards that, as the turn, make a straight
   * (or SF/royal) using exactly 2 hole + 3 of the 4 board cards. `nutOuts` are those
   * that are also the nut Omaha holding on that 4-card board. A hand that already holds a
   * straight or better on the flop is not drawing and returns `{ outs: 0, nutOuts: 0 }`.
   */
  omahaWrapDrawOuts(heroHoleCards: CardInput, flopCards: CardInput): OmahaWrapDrawResult;

  /**
   * 0–1 closeness to the nuts: `1 − (strictly better 4-card holdings) / n` over villain holdings
   * that exclude hero's cards, the board, and `extraDead`. Nuts → 1; unique worst → 0.
   */
  omahaNuttednessScore(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    extraDead?: CardInput
  ): number;

  /**
   * Monte Carlo pot-share equity for 3–4 known 4-card hands. Returned array sums to ~1.
   */
  omahaMultiwayEquityMc(
    holeHands: CardInput[],
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number[];

  /**
   * Best 5-card Big O hand: exactly 2 hole + exactly 3 board.
   * `holeCards` must be 5 cards; `boardCards` must be 3–5 (flop C(5,2)=10, river C(5,2)*C(5,3)=100).
   * Same `HandEvalResult` shape as `evaluateBestHand`.
   */
  evaluateBigOBestHand(
    holeCards: CardInput,
    boardCards: CardInput,
    options?: EvaluateBestHandOptions
  ): HandEvalResult;
  evaluateBigOBestHand(
    holeCards: CardInput,
    boardCards: CardInput,
    options: { format: 'slim' }
  ): HandEvalResultSlim;

  /**
   * Big O strength using the same `pack_hand_strength` encoding as `evaluateHandStrength`
   * (5-card rank + kickers). Not a 10-card best-of-N.
   */
  evaluateBigOHandStrength(holeCards: CardInput, boardCards: CardInput): number;

  /**
   * Exact HU Big O equity vs a known 5-card hand. Board 0–5; remaining runouts enumerated.
   * Ties count as 0.5.
   */
  exactHuBigOEquityVsKnown(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput
  ): number;

  /** Monte Carlo Big O equity vs a uniform random 5-card villain. */
  simulateBigOEquityVsRandom(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number;

  /** Monte Carlo Big O equity vs a sparse weighted 5-card range (`BigORangeSpec`). */
  simulateBigOEquityVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: BigORangeSpec,
    trials: number,
    seed: number
  ): number;

  /** Remaining 5-card combo count: `C(52 − |unique dead|, 5)`. Throws on duplicate dead cards. */
  bigOComboCount(deadCards: CardInput): number;

  /**
   * True iff hero’s Big O hand is unbeaten by every other 5-card combo that avoids
   * `boardCards` and optional `extraDead`.
   */
  bigONutsOnBoard(heroHoleCards: CardInput, boardCards: CardInput, extraDead?: CardInput): boolean;

  /**
   * Monte Carlo pot-share equity for 3–4 known 5-card hands. Returned array sums to ~1.
   */
  bigOMultiwayEquityMc(
    holeHands: CardInput[],
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number[];

  /**
   * 3-max Spin & Go prize vector: `multiplier * buyin`.
   * Default 50/30/20 of the pool. `winnerTakeAll` → 100/0/0. Always length 3.
   */
  spinGoPayouts(
    multiplier: number,
    buyin: number,
    winnerTakeAll?: boolean,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;

  /** Harville ICM $EV for three stacks and a length-3 Spin & Go prize vector. */
  spinGoIcmEv(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    returnFormat?: F64ReturnFormat
  ): number[] | Float64Array;

  /**
   * Average-position FGS orbits, then ICMBU on surviving stacks.
   * `orbits === 0` matches `pkoIcmbuPayouts` on the original stacks.
   */
  pkoFgsPayouts(
    stacks: F64VectorInput,
    payouts: F64VectorInput,
    bountyValues: F64VectorInput,
    orbits: number,
    smallBlind: number,
    bigBlind: number,
    ante?: number,
    returnFormat?: F64ReturnFormat
  ): PkoIcmbuResult;

  /**
   * Squeeze vs fold (chip EV). Fold = 0 (hero has not put chips in).
   * Independent folds vs opener and caller; continue pots add `heroPut` plus matching calls.
   */
  squeezeEv(
    pot: number,
    heroPut: number,
    openerCall: number,
    callerCall: number,
    foldEquityOpener: number,
    foldEquityCaller: number,
    equityVsOpener: number,
    equityVsCaller: number,
    equityVsBoth: number
  ): SqueezeEvResult;

  /**
   * 4-bet jam pot geometry (not ICM). Fold = 0. `foldEquity === 1` wins `deadPot`.
   * Called: `equity * (deadPot + jam + call) − jam`.
   */
  fourBetJamEv(
    deadPot: number,
    jam: number,
    call: number,
    foldEquity: number,
    equityWhenCalled: number
  ): FourBetJamEvResult;

  /**
   * SPR after the 3-bet. Realized equity = `equity * realization` (default 1).
   * Stack-off when realized equity covers `remaining / (pot + 2*remaining)`.
   */
  threeBetPotCommitEv(
    potAfterThreeBet: number,
    effectiveRemaining: number,
    equity: number,
    realization?: number
  ): ThreeBetCommitEvResult;

  /**
   * Short Deck / 6+ Hold'em. 36-card deck (ranks 6–A). Ranks 2–5 are rejected.
   * Flush beats full house. Wheel is A6789 (not A2345). `rankCategory` uses 6+ order
   * (fullHouse=5, flush=6), not NLHE order.
   */
  evaluateShortDeckBestHand(cards: CardInput): HandEvalResult;
  /** Encoded 6+ strength (flush ordinal above boat). Hole + board, 1–7 cards total. */
  evaluateShortDeckHandStrength(holeCards: CardInput, board: CardInput): number;
  /** 6+ category label (`highCard` … `royalFlush`); flush still named `flush`. */
  evaluateShortDeckCategory(holeCards: CardInput, board: CardInput): string;
  /** Exact HU equity on the 36-card deck. 2-card holes, board 0–5. Ties count 0.5. */
  exactHuShortDeckEquityVsKnown(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput
  ): number;
  simulateShortDeckEquityVsRandom(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    numSimulations: number,
    seed: number
  ): number;
  /**
   * MC vs a 6+ range. Valid classes are **81** (9 pairs + 36 suited + 36 offsuit), not 169
   * and not 91. Accepts `string[]` notations (`AKs`, `QQ`), `{ AKs: 1, QQ: 0.5 }`,
   * `Float64Array(81)`, or `Float64Array(169)` with 2–5 class weights required to be 0.
   */
  simulateShortDeckEquityVsRange(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    range: ShortDeckRangeInput,
    numSimulations: number,
    seed: number
  ): number;
  /** True when five cards are the A6789 wheel (straight or wheel straight flush). */
  shortDeckStraightIsWheel(cards: CardInput): boolean;
  /** `C(n,2)` remaining hole combos on the 36-card deck after dead cards. */
  shortDeckRemainingComboCount(deadCards: CardInput): number;
  /**
   * HU jam/fold Nash frequencies, length 81, stacks in BB. Same fictitious-play idea as
   * `nashHeadsUpJamRange` but 6+ equities.
   */
  shortDeckNashHuJamRange(
    stackBbOrOptions: number | ShortDeckNashOptions,
    smallBlind?: number,
    bigBlind?: number,
    ante?: number
  ): Float64Array;
  /**
   * True when NLHE vs 6+ **category labels** differ for the same 5–7 cards (all 6+).
   * Wheel A6789 is the usual flip (`highCard`/`flush` vs `straight`/`straightFlush`).
   * Flush vs boat does not rename a single hand; it only swaps which wins at showdown.
   */
  shortDeckVsHoldemCategoryFlip(cards: CardInput): boolean;

  /**
   * Best 8-or-better Omaha low: exactly 2 hole + 3 board. Ace is low.
   * Straights and flushes do not disqualify. Pair or any rank 9+ kills that 5-card combo.
   */
  evaluateOmahaLoHand(holeCards: CardInput, boardCards: CardInput): OmahaLoHand;

  /** True iff `evaluateOmahaLoHand` qualifies. */
  omahaLoQualifies(holeCards: CardInput, boardCards: CardInput): boolean;

  /**
   * Omaha Hi (same as `evaluateOmahaBestHand`) plus 8-or-better low.
   * Hole 4, board 3–5.
   */
  evaluateOmahaHiLo(holeCards: CardInput, boardCards: CardInput): OmahaHiLoHands;

  /**
   * Exact HU PLO-8 vs a known 4-card hand. Board 0–5.
   * No qualifying low → high takes the full pot.
   */
  exactHuOmahaHiLoEquity(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput
  ): OmahaHiLoEquity;

  /**
   * Monte Carlo PLO-8. `villainHoleCards` null → uniform random 4-card opponent.
   */
  simulateOmahaHiLoEquity(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput | null,
    boardCards: CardInput,
    trials: number,
    seed: number
  ): OmahaHiLoEquity;

  /** P(hero wins both halves, or wins high when no low exists). */
  omahaScoopProbabilityMc(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number;

  /** P(exactly one side is a tie and the other is won or lost). */
  omahaQuarterProbabilityMc(
    heroHoleCards: CardInput,
    villainHoleCards: CardInput,
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number;

  /** True iff hero has a qualifying low unbeaten by any other 4-card combo. */
  omahaLoNutsOnBoard(heroHoleCards: CardInput, boardCards: CardInput, extraDead?: CardInput): boolean;

  /**
   * `scoopNuts` is nut high and nut low, or nut high when the board cannot make a low
   * (high takes the whole pot).
   */
  omahaHiLoNuttedness(
    heroHoleCards: CardInput,
    boardCards: CardInput,
    extraDead?: CardInput
  ): OmahaHiLoNuttedness;

  /**
   * 3-way chip EV with hi/lo halves and quartering. Exactly 3 known 4-card hands.
   * No qualifying low → high takes the full pot, split among high winners.
   */
  omahaHiLoMultiwayMc(
    holeHands: CardInput[],
    boardCards: CardInput,
    trials: number,
    seed: number
  ): number[];

  /**
   * Kansas City 2-7 single draw. Ace high; straights and flushes are bad.
   * Lower packed strength is the better low.
   */
  evaluateDeuceSevenHand(cards: CardInput): number;
  evaluateDeuceSevenCategory(cards: CardInput): DeuceSevenCategory;
  /**
   * Unpaired, no straight, no flush. Default `eightPat=true` treats 8-high as pat;
   * `false` requires 7-high.
   */
  deuceSevenIsPat(cards: CardInput, eightPat?: boolean | { eightPat?: boolean }): boolean;
  /**
   * Hero equity after both stand or draw (ties 0.5). Omit options to stand pat.
   * Exact replacement tree when small; otherwise Monte Carlo.
   */
  deuceSevenDrawEquityVsKnown(
    hero: CardInput,
    villain: CardInput,
    options?: DeuceSevenDrawOptions
  ): number;
  /** True iff 7-5-4-3-2 unpaired unsuited. */
  deuceSevenNutsPat(cards: CardInput): boolean;
  /** Both hands must be unpaired 8-high. Smooth = second card is not a 7. */
  deuceSevenRoughVsSmooth(a: CardInput, b: CardInput): DeuceSevenRoughSmooth;
  /** Pot-share vector for 2–8 disjoint 5-card hands. Ties split. */
  deuceSevenMultiwayShowdown(hands: CardInput[]): number[];
  /**
   * 7-card stud hi. Best 5-card high from **3–7** cards. Same `HandEvalResult` as
   * `evaluateBestHand`.
   */
  evaluateStudBestHand(cards: CardInput, options?: EvaluateBestHandOptions): HandEvalResult;
  evaluateStudBestHand(cards: CardInput, options: { format: 'slim' }): HandEvalResultSlim;

  /**
   * Razz (7-card A-to-5 low). Aces low. Straights and flushes do **not** count.
   * Best 5 from **3–7** cards. Lower `strength` is better. Wheel is A2345.
   * `kickers` are ace-low values (A=0 … K=12), unlike the hold'em evaluators; hands with fewer
   * than 5 cards pad with 13. Distinct from 2-7 lowball.
   */
  evaluateRazzHand(cards: CardInput, options?: EvaluateBestHandOptions): HandEvalResult;
  evaluateRazzHand(cards: CardInput, options: { format: 'slim' }): HandEvalResultSlim;

  /** True iff the best 5-card A-5 low is A2345 (suited wheel still counts). */
  razzWheelIsNuts(cards: CardInput): boolean;

  /**
   * Exact HU 7-card stud equity vs a known hand. Each side 3–7 cards; remaining
   * streets come from the dead-aware deck. Both already 7 → one comparison.
   * Ties count 0.5. `extraDead` = folded upcards.
   */
  exactHuStudEquityVsKnown(
    heroCards: CardInput,
    villainCards: CardInput,
    extraDead?: CardInput
  ): number;

  /** Exact HU razz equity. Same dealing rules as stud; lower A-5 low wins. */
  exactHuRazzEquityVsKnown(
    heroCards: CardInput,
    villainCards: CardInput,
    extraDead?: CardInput
  ): number;

  /** Remaining deck after holes + upcards. Canonical strings, deck-index order. */
  studDeadCardDeck(deadCards: CardInput): string[];

  /** Monte Carlo stud hi: random matching-street villain, complete both to 7. */
  simulateStudEquityVsRandom(
    heroCards: CardInput,
    trials: number,
    seed: number,
    extraDead?: CardInput
  ): number;

  /** Monte Carlo razz: random matching-street villain, complete both to 7. */
  simulateRazzEquityVsRandom(
    heroCards: CardInput,
    trials: number,
    seed: number,
    extraDead?: CardInput
  ): number;
}


declare const api: PokerCalculations;
export = api;

