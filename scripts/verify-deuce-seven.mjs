/**
 * Kansas City 2-7 single draw export checks.
 * Run from NPM/: `node scripts/verify-deuce-seven.mjs` (requires built native addon).
 *
 * Ace is high; straights and flushes count against you. Best hand is 7-5-4-3-2 rainbow.
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
  'evaluateDeuceSevenHand',
  'evaluateDeuceSevenCategory',
  'deuceSevenIsPat',  'deuceSevenDrawEquityVsKnown',  'deuceSevenNutsPat',
  'deuceSevenRoughVsSmooth',
  'deuceSevenMultiwayShowdown',];
for (const n of names) {
  assertTrue(`${n} exported`, typeof poker[n] === 'function');
}

const wheel = ['7c', '5d', '4h', '3s', '2c'];
const sevenSix = ['7d', '6c', '4s', '3h', '2d'];
assertTrue('75432 is nuts', poker.deuceSevenNutsPat(wheel) === true);
assertTrue('76432 is not nuts', poker.deuceSevenNutsPat(sevenSix) === false);
assertTrue(
  '75432 offsuit beats 76432',
  poker.evaluateDeuceSevenHand(wheel) < poker.evaluateDeuceSevenHand(sevenSix),
);
assertTrue('75432 category is nuts', poker.evaluateDeuceSevenCategory(wheel) === 'nuts');

const aceLow = ['As', '5c', '4d', '3c', '2h'];
const eightSix = ['8c', '6d', '4c', '3d', '2s'];
assertTrue(
  'A5432 (Ace high) worse than 86432',
  poker.evaluateDeuceSevenHand(aceLow) > poker.evaluateDeuceSevenHand(eightSix),
);
assertTrue('A5432 is number (Ace-high), not a wheel straight', poker.evaluateDeuceSevenCategory(aceLow) === 'number');
assertTrue('65432 is a straight (bad)', poker.evaluateDeuceSevenCategory(['6h', '5s', '4d', '3c', '2h']) === 'straight');

assertTrue(
  'flush 75432 worse than unpaired 7',
  poker.evaluateDeuceSevenHand(['7h', '5h', '4h', '3h', '2h']) > poker.evaluateDeuceSevenHand(wheel),
);
assertTrue('suited 75432 is flush', poker.evaluateDeuceSevenCategory(['7h', '5h', '4h', '3h', '2h']) === 'flush');

assertTrue('8-high is pat by default', poker.deuceSevenIsPat(eightSix) === true);
assertTrue('8-high is not pat when eightPat=false', poker.deuceSevenIsPat(eightSix, false) === false);
assertTrue('7-high is pat even without eightPat', poker.deuceSevenIsPat(wheel, false) === true);
assertTrue('pair is not pat', poker.deuceSevenIsPat(['7c', '7d', '4h', '3s', '2c']) === false);

const smooth8 = ['8c', '6d', '5h', '3s', '2c'];
const rough8 = ['8d', '7c', '5s', '4c', '2h'];
const rs = poker.deuceSevenRoughVsSmooth(smooth8, rough8);
assertTrue('smooth 8 beats rough 8', rs.cmp === -1);
assertTrue('smooth flag on 86532', rs.aSmooth === true);
assertTrue('rough flag on 87542', rs.bSmooth === false);

const mw = poker.deuceSevenMultiwayShowdown([wheel, sevenSix, eightSix]);
assertNear('nuts scoops multiway', mw[0], 1);
assertNear('others get 0', mw[1] + mw[2], 0);

const stand = poker.deuceSevenDrawEquityVsKnown(wheel, sevenSix);
assertNear('nuts vs 76432 standing is 1', stand, 1);
const chop = poker.deuceSevenDrawEquityVsKnown(wheel, ['7h', '5c', '4d', '3c', '2h']);
assertNear('two wheels chop', chop, 0.5);

assertThrows('duplicate cards throw', () => poker.evaluateDeuceSevenHand(['7c', '5d', '4h', '3s', '7c']));
assertThrows('overlap throws on rough-vs-smooth', () =>
  poker.deuceSevenRoughVsSmooth(smooth8, ['8c', '6h', '4d', '3c', '2d']),
);
assertThrows('pair of eights is not an 8-high', () =>
  poker.deuceSevenRoughVsSmooth(['8h', '8s', '5d', '4s', '2s'], rough8),
);

const drawBoth = poker.deuceSevenDrawEquityVsKnown(['9c', '8d', '7h', '2s', 'Ac'], ['Kc', 'Qd', 'Jh', 'Ts', '9d'], {
  heroDiscard: 1,
  villainDiscard: 0,
  trials: 40,
  seed: 2,
});
assertTrue('draw equity in [0,1]', drawBoth >= 0 && drawBoth <= 1);

console.log('OK: verify-deuce-seven.mjs — 75432 beats 76432; Ace high; flush worse; dump Ace on 7543A.');
