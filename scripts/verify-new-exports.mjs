/**
 * Runtime checks for the native public export surface.
 * Run from NPM/: `node scripts/verify-new-exports.mjs` (requires built native addon).
 */
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(join(root, 'package.json'));

let poker;
try {
  poker = require('./index.js');
} catch (e) {
  console.error('Native addon not loaded — run `npm run build:native` first.');
  console.error(e.message);
  process.exit(1);
}

const EPS = 1e-9;
function assertNear(label, actual, expected, tol = EPS) {
  if (Math.abs(actual - expected) > tol) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}
function assertTrue(label, cond) {
  if (!cond) throw new Error(label);
}

// --- single-street draws ---
assertNear('flopToTurnAtLeastOneHitProbability', poker.flopToTurnAtLeastOneHitProbability(9, 47), 9 / 47);
assertNear('turnToRiverAtLeastOneHitProbability', poker.turnToRiverAtLeastOneHitProbability(9, 44), 9 / 44);
assertNear(
  'flopToTurnAtLeastOneHitUnionTwoCategories',
  poker.flopToTurnAtLeastOneHitUnionTwoCategories(47, 9, 4, 1),
  12 / 47,
);
assertNear(
  'turnToRiverAtLeastOneHitUnionTwoCategories',
  poker.turnToRiverAtLeastOneHitUnionTwoCategories(44, 9, 4, 1),
  12 / 44,
);
assertNear(
  'flopToTurnAtLeastOneHitDisjointOutsSum',
  poker.flopToTurnAtLeastOneHitDisjointOutsSum(47, [9, 4]),
  13 / 47,
);
assertNear(
  'turnToRiverAtLeastOneHitDisjointOutsSum',
  poker.turnToRiverAtLeastOneHitDisjointOutsSum(44, [9, 4]),
  13 / 44,
);
const twoCard = poker.hypergeometricTwoCardHitProbability(9, 47);
assertNear('hypergeometricTwoCardHitProbability', twoCard, poker.flopToRiverAtLeastOneHitProbability(9, 47));
assertNear(
  'hypergeometricTwoCardMiss',
  poker.hypergeometricTwoCardHitProbability(9, 47) + poker.hypergeometricTwoCardMissProbability(9, 47),
  1,
);
assertTrue(
  'runnerRunnerBackdoorFlushOneCard <= twoCard',
  poker.runnerRunnerBackdoorFlushOneCardProbability(9, 47) <=
    poker.runnerRunnerBackdoorFlushTwoCardProbability(9, 47) + EPS,
);
assertNear('blockerAdjustedOuts(0 frac)', poker.blockerAdjustedOuts(9, 0), 9);
assertNear('blockerAdjustedOuts(1 frac)', poker.blockerAdjustedOuts(9, 1), 0);
assertTrue('suitBlockerFraction in [0,1]', poker.suitBlockerFraction(2, 47) >= 0 && poker.suitBlockerFraction(2, 47) <= 1);

// --- pot / rake ---
const pot = 100;
const rakeF = 0.05;
const rakeC = 10;
const rake = poker.rakeFromPot(pot, rakeF, rakeC);
assertNear('netPotAfterRake', poker.netPotAfterRake(pot, rakeF, rakeC), pot - rake);
const toCall = 50;
const netAfterCall = poker.netPotAfterCallAndRake(100, toCall, rakeF, rakeC);
assertNear(
  'effectivePotOdds inverse breakevenCallEquityWithRake',
  poker.effectivePotOddsDisplayAfterRake(100, toCall, rakeF, rakeC),
  1 / poker.breakevenCallEquityWithRake(100, toCall, rakeF, rakeC),
  1e-6,
);
assertNear('impliedBreakevenTotalPot', poker.impliedBreakevenTotalPot(100, 50, 0.25), (100 + 50) / 0.25);
assertNear(
  'impliedOdds round-trip',
  poker.impliedOddsRequiredEquityFromFutureWin(100, 50, 100),
  poker.breakevenCallEquity(100 + 100, 50),
  1e-6,
);
assertNear('expectedValueRaise FE=1', poker.expectedValueRaise(0.5, 100, 50, 1, 200), 100);
assertNear(
  'expectedValueRaise FE=0',
  poker.expectedValueRaise(0.5, 100, 50, 0, 200),
  0.5 * 200 - 50,
);
assertTrue(
  'expectedValueRaiseWithRake <= without',
  poker.expectedValueRaiseWithRake(0.5, 100, 50, 0, 200, rakeF, rakeC) <=
    poker.expectedValueRaise(0.5, 100, 50, 0, 200) + EPS,
);
const beRaiseEq = poker.breakevenRaiseEquity(100, 50, 0.3, 200);
assertNear(
  'breakevenRaiseEquity EV≈0',
  poker.expectedValueRaise(beRaiseEq, 100, 50, 0.3, 200),
  0,
  1e-6,
);
assertTrue(
  'breakevenCallEquityWithPostedAnte >= breakevenCallEquity',
  poker.breakevenCallEquityWithPostedAnte(100, 50, 10) >= poker.breakevenCallEquity(100, 50) - EPS,
);
assertNear('potSizeAfterHuCall', poker.potSizeAfterHuCall(100, 50), 200);
assertNear('potSizeAfterHuBet', poker.potSizeAfterHuBet(100, 50), 200);
assertNear('expectedValuePerBigBlind', poker.expectedValuePerBigBlind(10, 2), 5);

// --- GTO with rake ---
const mdfRake = poker.minimumDefenseFrequencyWithRake(100, 50, rakeF, rakeC);
assertTrue('MDF with rake >= MDF', mdfRake >= poker.minimumDefenseFrequency(100, 50) - EPS);
assertNear('alpha+MDF with rake', mdfRake + poker.alphaFrequencyWithRake(100, 50, rakeF, rakeC), 1);
assertTrue(
  'bluffToValueRatioWithRake <= without',
  poker.bluffToValueRatioWithRake(100, 50, rakeF, rakeC) <= poker.bluffToValueRatio(100, 50) + EPS,
);
assertNear(
  'valueToBluff inverse',
  poker.valueToBluffRatioWithRake(100, 50, rakeF, rakeC),
  1 / poker.bluffToValueRatioWithRake(100, 50, rakeF, rakeC),
  1e-6,
);

// --- sizing ---
assertNear('sprAfterBet mirrors sprAfterCall', poker.sprAfterBet(100, 50, 200), poker.sprAfterCall(100, 50, 200));
assertNear('sprAfterRaise', poker.sprAfterRaise(100, 50, 200), poker.sprAfterBet(100, 50, 200));
assertNear('commitmentRatioAfterBet', poker.commitmentRatioAfterBet(50, 200), 0.25);
const frac = 0.5;
const bet = poker.betSizeToMatchPotFraction(100, frac);
assertNear('betSize round-trip', poker.betAsPotFraction(100, bet), frac, 1e-6);

// --- Kelly ---
const p = 0.55;
const b = 1;
const k = poker.kellyCriterionBinary(p, b);
assertNear('halfKelly', poker.halfKellyCriterionBinary(p, b), Math.max(0, k / 2));
assertNear('quarterKelly', poker.quarterKellyCriterionBinary(p, b), Math.max(0, k / 4));
assertNear('eighthKelly', poker.eighthKellyCriterionBinary(p, b), Math.max(0, k / 8));
const kClamped = poker.kellyCriterionBinaryClamped(p, b);
assertTrue('kelly clamped [0,1]', kClamped >= 0 && kClamped <= 1);

// --- fold equity ---
assertTrue(
  'breakevenFoldEquityPureBluffWithAnte >= without',
  poker.breakevenFoldEquityPureBluffWithAnte(100, 50, 10) >=
    poker.breakevenFoldEquityPureBluff(100, 50) - EPS,
);
assertTrue(
  'twoStreetPureBluffEvWithRake <= without',
  poker.twoStreetPureBluffEvWithRake(100, 50, 75, 0.4, 0.5, rakeF, rakeC) <=
    poker.twoStreetPureBluffEv(100, 50, 75, 0.4, 0.5) + EPS,
);

// --- multiway ---
assertTrue(
  'multiwaySymmetricBreakevenCallEquityWithRake >= without',
  poker.multiwaySymmetricBreakevenCallEquityWithRake(100, 50, 2, rakeF, rakeC) >=
    poker.multiwaySymmetricBreakevenCallEquity(100, 50, 2) - EPS,
);
assertNear(
  'multiwayExpectedValueCall k=0',
  poker.multiwayExpectedValueCall(0.4, 100, 50, 0),
  poker.expectedValueCall(0.4, 100, 50),
);

// --- reverse implied ---
assertNear(
  'reverseImpliedOddsMinEquity round-trip',
  poker.reverseImpliedOddsMinEquity(100, 50, 200),
  poker.breakevenCallEquity(100 + 200, 50),
  1e-6,
);
assertNear('geometricPotAfterSingleMatchedBet', poker.geometricPotAfterSingleMatchedBet(100, 50), 200);

// --- statistics ---
const wilson = poker.wilsonScoreInterval(50, 100, 1.96);
assertNear(
  'binomialProportionCiWidth',
  poker.binomialProportionCiWidth(50, 100, 1.96),
  wilson.upper - wilson.lower,
  1e-6,
);
const nTrials = poker.monteCarloTrialsForWilsonHalfWidth(0.5, 0.05, 1.96);
const w2 = poker.wilsonScoreInterval(Math.floor(0.5 * nTrials), nTrials, 1.96);
assertTrue('monteCarloTrialsForWilsonHalfWidth', (w2.upper - w2.lower) / 2 <= 0.05 + 1e-3);
assertNear('varianceToStandardDeviationPerHand', poker.varianceToStandardDeviationPerHand(4), 2);

// --- ICM ---
const payouts = [100, 50, 30];
const stacks = [5000, 3000, 2000];
const equal = poker.icmEqualChopPayouts(payouts);
assertNear('icmEqualChop sum', equal.reduce((a, b) => a + b, 0), 180);
const surplus = poker.icmChopSurplusVsEqualSplit(stacks, payouts);
assertNear('icmChopSurplus sum≈0', surplus.reduce((a, b) => a + b, 0), 0, 1e-6);
assertNear('icmTotalPrizePool', poker.icmTotalPrizePool(payouts), 180);
const evPerChip = poker.icmDealEvPerChip(stacks, payouts);
assertTrue('icmDealEvPerChip positive', evPerChip.every((x) => x > 0));
const sat = poker.icmSatelliteAdvanceProbability(stacks, 2);
assertTrue('satellite probs in [0,1]', sat.every((x) => x >= 0 && x <= 1));
assertNear('icmPayoutStructureGini equal', poker.icmPayoutStructureGini([50, 50, 50]), 0, 1e-6);

// --- side pots ---
const commits = [100, 200, 500];
assertNear(
  'sidePotLayerCount',
  poker.sidePotLayerCount(commits),
  poker.sidePotLadderFromCommitments(commits).length,
);
assertNear('sidePotBreakevenCallEquity', poker.sidePotBreakevenCallEquity(100, 50), 50 / 150);

// --- stacks ---
assertTrue(
  'preflopCombosFromNotationMinusBlockers <= full',
  poker.preflopCombosFromNotationMinusBlockers('AKs', 2) <= poker.preflopCombosFromNotation('AKs'),
);
const spr = poker.sprAfterCall(100, 50, 200);
assertNear('stackToPotAfterCall round-trip', poker.stackToPotAfterCall(100, 50, 200), 1 / spr, 1e-6);

// --- exact made (smoke) ---
const hero = ['Ah', 'Kh'];
const flop = ['2h', '7d', '9c'];
const dead = [];
for (const fn of [
  'flushMadeFlopToRiverExactProbability',
  'fullHouseMadeFlopToRiverExactProbability',
  'tripsMadeFlopToRiverExactProbability',
  'twoPairMadeFlopToRiverExactProbability',
]) {
  const p = poker[fn](hero, flop, dead);
  assertTrue(`${fn} in [0,1]`, p >= 0 && p <= 1);
}

// --- subgame ---
assertNear(
  'pushFoldSymmetricEv',
  poker.pushFoldSymmetricEv(0.5, 100, 20),
  poker.chubukovSymmetricJamEv(0.5, 100, 20),
);
assertNear(
  'openRaiseBreakevenFoldEquity',
  poker.openRaiseBreakevenFoldEquity(100, 50),
  poker.breakevenFoldEquityPureBluff(100, 50),
);
assertNear('callOrFoldChipEvDelta', poker.callOrFoldChipEvDelta(0.4, 100, 50), poker.expectedValueCall(0.4, 100, 50));
assertNear('normalizedRangeWeightSum', poker.normalizedRangeWeightSum([1, 2, 3]), 6);

// --- range, board, and policy diagnostics ---
const dense = new Float64Array(1326);
dense.fill(1 / 1326);
assertNear('normalizeSparseRange sum', poker.normalizedRangeWeightSum(poker.normalizeSparseRange(dense)), 1, 1e-6);
assertTrue('rangeTopCombos rows', poker.rangeTopCombos(dense, 3).length === 3);
assertTrue('boardWetnessScore in [0,1]', poker.boardWetnessScore(['Qh', 'Jh', '2c']) >= 0 && poker.boardWetnessScore(['Qh', 'Jh', '2c']) <= 1);
assertTrue('boardTextureScore has wetness', typeof poker.boardTextureScore(['Qh', 'Jh', '2c']).wetness === 'number');
assertTrue('cbetSizeEvGrid rows', poker.cbetSizeEvGrid(dense, dense, ['Qh', 'Jh', '2c'], 100, [33, 66]).rows.length === 2);
assertTrue('opponentAggressionFactor number', typeof poker.opponentAggressionFactor(3, 2, 5) === 'number');
const state = {
  players: [
    { holeCards: ['Ah', 'Kh'], stack: 200, seat: 0, committedThisStreet: 0 },
    { holeCards: ['7c', '7d'], stack: 180, seat: 1, folded: false },
  ],
  communityCards: ['Qh', 'Jh', '2c'],
  phase: 'flop',
  pot: 30,
  currentBet: 10,
  smallBlind: 1,
  bigBlind: 2,
  actingIndex: 0,
  actedThisStreet: [false, true],
};
assertTrue('legalActionSummary', typeof poker.legalActionSummary(state).toCall === 'number');

// --- export count ---
const reg = readFileSync(join(root, 'native', 'binding_register.cpp'), 'utf8');
const regCount = (reg.match(/PropertyDescriptor::Function/g) || []).length;
assertTrue('binding_register count 300', regCount === 300);
const exportCount = Object.keys(poker).filter((k) => typeof poker[k] === 'function').length;
assertTrue('runtime export count 300', exportCount === 300);

console.log('OK: verify-new-exports.mjs - all checks passed.');
