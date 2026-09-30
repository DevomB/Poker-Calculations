/**
 * Omaha Hi-Lo (PLO-8) export checks.
 * Run from NPM/: `node scripts/verify-omaha-hi-lo.mjs` (requires built native addon).
 *
 * Wheel: As 2c 9h Kd on 3d 4h 5s Tc Jc → A-2-3-4-5, Ace low, qualifies.
 * Pair: As 2c Kh Kd on 2h 3d 4s 9c Tc → using the deuce pairs the board; no low.
 * No-low paired broadway: hi takes the full pot (potShare === hiEquity).
 */
import { createRequire } from 'node:module';
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
function assertThrows(label, fn) {
  let threw = false;
  try {
    fn();
  } catch {
    threw = true;
  }
  if (!threw) throw new Error(`${label}: expected throw`);
}

const names = [
  'evaluateOmahaLoHand',
  'omahaLoQualifies',
  'evaluateOmahaHiLo',
  'exactHuOmahaHiLoEquity',
  'simulateOmahaHiLoEquity',
  'omahaScoopProbabilityMc',
  'omahaQuarterProbabilityMc',
  'omahaLoNutsOnBoard',
  'omahaHiLoNuttedness',
  'omahaHiLoMultiwayMc',
];
for (const n of names) {
  assertTrue(`${n} exported`, typeof poker[n] === 'function');
}

const wheelHero = ['As', '2c', '9h', 'Kd'];
const wheelBoard = ['3d', '4h', '5s', 'Tc', 'Jc'];
const wheel = poker.evaluateOmahaLoHand(wheelHero, wheelBoard);
assertTrue('aces-low wheel qualifies', wheel.qualifies === true);
assertTrue('omahaLoQualifies matches', poker.omahaLoQualifies(wheelHero, wheelBoard) === true);
assertTrue(
  'wheel ranks are 5-4-3-2-1 (Ace=1)',
  Array.isArray(wheel.ranks) &&
    wheel.ranks[0] === 5 &&
    wheel.ranks[1] === 4 &&
    wheel.ranks[2] === 3 &&
    wheel.ranks[3] === 2 &&
    wheel.ranks[4] === 1,
);

const pairHero = ['As', '2c', 'Kh', 'Kd'];
const pairBoard = ['2h', '3d', '4s', '9c', 'Tc'];
const paired = poker.evaluateOmahaLoHand(pairHero, pairBoard);
assertTrue('pair kills low', paired.qualifies === false);
assertTrue('omahaLoQualifies false when paired', poker.omahaLoQualifies(pairHero, pairBoard) === false);

const hiLo = poker.evaluateOmahaHiLo(wheelHero, wheelBoard);
assertTrue('hi is Omaha Hi 2+3', typeof hiLo.hi.rank === 'string' && Number.isFinite(hiLo.hi.strength));
assertTrue('hi matches evaluateOmahaBestHand', hiLo.hi.strength === poker.evaluateOmahaBestHand(wheelHero, wheelBoard).strength);
assertTrue('lo wheel in hi/lo', hiLo.lo.qualifies === true && hiLo.lo.ranks[4] === 1);

const scoopHero = ['As', '2c', 'Ah', 'Kh'];
const scoopVil = ['9d', '9c', '8d', '7c'];
const scoopBoard = ['3d', '4h', '5s', 'Ac', 'Qc'];
const exact = poker.exactHuOmahaHiLoEquity(scoopHero, scoopVil, scoopBoard);
assertTrue('scoop equity in [0,1]', exact.scoopEquity >= 0 && exact.scoopEquity <= 1);
assertTrue('hi equity in [0,1]', exact.hiEquity >= 0 && exact.hiEquity <= 1);
assertTrue('lo equity in [0,1]', exact.loEquity >= 0 && exact.loEquity <= 1);
assertTrue('quarter rate in [0,1]', exact.quarterRate >= 0 && exact.quarterRate <= 1);
assertTrue('pot share in [0,1]', exact.potShare >= 0 && exact.potShare <= 1);
assertNear('hero scoops this river', exact.scoopEquity, 1);
assertNear('hero pot share 1 when scooping', exact.potShare, 1);
assertNear('flip pot shares sum to 1', exact.potShare + poker.exactHuOmahaHiLoEquity(scoopVil, scoopHero, scoopBoard).potShare, 1);

const noLowHero = ['As', 'Ah', 'Kh', 'Kd'];
const noLowVil = ['Qs', 'Qh', 'Js', 'Jd'];
const noLowBoard = ['9c', '9d', 'Tc', 'Th', '2s'];
const noLow = poker.exactHuOmahaHiLoEquity(noLowHero, noLowVil, noLowBoard);
assertTrue('no qualifying low', poker.omahaLoQualifies(noLowHero, noLowBoard) === false);
assertTrue('villain also no low', poker.omahaLoQualifies(noLowVil, noLowBoard) === false);
assertNear('no-low: hi takes full pot', noLow.potShare, noLow.hiEquity);
assertNear('no-low: lo equity is 0', noLow.loEquity, 0);
assertNear('no-low: quarter rate 0', noLow.quarterRate, 0);
assertNear('AAKK wins hi vs QQJJ on this board', noLow.hiEquity, 1);
assertNear('no-low scoop is the hi win', noLow.scoopEquity, 1);

assertThrows('duplicate hole cards throw', () =>
  poker.evaluateOmahaLoHand(['As', 'As', '2c', '3d'], wheelBoard),
);
assertThrows('hole/board overlap throw', () =>
  poker.evaluateOmahaLoHand(['As', '2c', '3d', '4h'], wheelBoard),
);
assertThrows('exact HU duplicate throw', () =>
  poker.exactHuOmahaHiLoEquity(['As', '2c', '3d', '4h'], ['As', '5c', '6d', '7h'], wheelBoard),
);

const mc = poker.simulateOmahaHiLoEquity(scoopHero, scoopVil, scoopBoard, 40, 7);
assertTrue('MC scoop in [0,1]', mc.scoopEquity >= 0 && mc.scoopEquity <= 1);
assertNear('MC river scoop is 1', mc.scoopEquity, 1);
const vsRand = poker.simulateOmahaHiLoEquity(scoopHero, null, scoopBoard, 60, 3);
assertTrue('random MC scoop in [0,1]', vsRand.scoopEquity >= 0 && vsRand.scoopEquity <= 1);
assertTrue('random MC pot share in [0,1]', vsRand.potShare >= 0 && vsRand.potShare <= 1);

const scoopP = poker.omahaScoopProbabilityMc(scoopHero, scoopVil, scoopBoard, 30, 1);
assertTrue('omahaScoopProbabilityMc in [0,1]', scoopP >= 0 && scoopP <= 1);
assertNear('river scoop MC is 1', scoopP, 1);
const qP = poker.omahaQuarterProbabilityMc(scoopHero, scoopVil, scoopBoard, 30, 1);
assertTrue('omahaQuarterProbabilityMc in [0,1]', qP >= 0 && qP <= 1);
assertNear('full scoop is not a quarter', qP, 0);

assertTrue('nut low on wheel board', poker.omahaLoNutsOnBoard(wheelHero, wheelBoard) === true);
assertTrue('paired hole is not nut low', poker.omahaLoNutsOnBoard(pairHero, pairBoard) === false);

const nuts = poker.omahaHiLoNuttedness(scoopHero, scoopBoard);
assertTrue('nuttedness has three flags', typeof nuts.hiNuts === 'boolean' && typeof nuts.loNuts === 'boolean' && typeof nuts.scoopNuts === 'boolean');
assertTrue('scoop nuts implies hi nuts or no-low scoop', nuts.scoopNuts === false || nuts.hiNuts === true);

const third = ['Ts', 'Th', 'Js', 'Jh'];
const mw = poker.omahaHiLoMultiwayMc([scoopHero, scoopVil, third], scoopBoard, 40, 2);
assertNear('3-way chip EV sums to 1', mw.reduce((a, b) => a + b, 0), 1, 1e-9);
assertTrue('hero scoops 3-way river', mw[0] > 0.99);
assertThrows('multiway needs 3 hands', () =>
  poker.omahaHiLoMultiwayMc([scoopHero, scoopVil], scoopBoard, 10, 1),
);

console.log('OK: verify-omaha-hi-lo.mjs — wheel qualifies; pair kills low; scoop in [0,1]; no-low hi takes pot; dups throw.');
