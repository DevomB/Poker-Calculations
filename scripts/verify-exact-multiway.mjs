/**
 * Exact multiway known-hand equity checks.
 * Run from NPM/: `node scripts/verify-exact-multiway.mjs` (requires built native addon).
 */
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
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

function sum(xs) {
  return xs.reduce((a, b) => a + b, 0);
}

const names = [
  'exactThreeWayEquityKnownHands',
  'exactThreeWayWinTieLoseKnownHands',
  'exactFourWayEquityKnownHands',
  'exactMultiwayEquityKnownHands',
  'exactMultiwayEquityWithDeadCards',
  'exactMultiwaySidePotChipEv',
  'exactMultiwayAheadFrequency',
  'exactMultiwayTieFrequency',
  'exactMultiwayRunoutCount',
  'exactMultiwayBestWorstRunout',
];
for (const name of names) {
  assertTrue(`${name} exported`, typeof poker[name] === 'function');
}

const aa = ['As', 'Ad'];
const kk = ['Ks', 'Kd'];
const qq = ['Qs', 'Qd'];
const twoSevenOff = ['7h', '2c'];
const twoSevenOffB = ['7d', '2s'];

const aaKkQq = poker.exactThreeWayEquityKnownHands(aa, kk, qq, []);
assertTrue('AA vs KK vs QQ length 3', aaKkQq.length === 3);
assertNear('AA vs KK vs QQ sum', sum(aaKkQq), 1, 1e-9);
assertTrue('AA > KK preflop 3-way', aaKkQq[0] > aaKkQq[1]);
assertTrue('KK > QQ preflop 3-way', aaKkQq[1] > aaKkQq[2]);
assertNear(
  'generic 3-way matches named',
  sum(poker.exactMultiwayEquityKnownHands([aa, kk, qq], []).map((e, i) => Math.abs(e - aaKkQq[i]))),
  0,
  1e-12,
);

const aaVsAir = poker.exactThreeWayEquityKnownHands(aa, twoSevenOff, twoSevenOffB, []);
assertNear('AA vs 72o vs 72o sum', sum(aaVsAir), 1, 1e-9);
assertTrue('AA huge vs two 72o', aaVsAir[0] > 0.8);

let dupThrew = false;
try {
  poker.exactThreeWayEquityKnownHands(['As', 'Ad'], ['As', 'Kd'], ['Qs', 'Qd'], []);
} catch {
  dupThrew = true;
}
assertTrue('duplicate cards throw', dupThrew);

const nutFlop = ['4h', '5h', '6h'];
const nutHoles = [
  ['7h', '8h'],
  ['2c', '2d'],
  ['3c', '3d'],
];
const nutEq = poker.exactMultiwayEquityKnownHands(nutHoles, nutFlop);
assertNear('flop nuts equity sum', sum(nutEq), 1, 1e-9);
assertTrue('flop nuts near 1', nutEq[0] > 0.99);

const riverBoard = ['2h', '7c', '9d', 'Td', '5s'];
assertTrue(
  'river runout count is 1',
  poker.exactMultiwayRunoutCount([aa, kk, qq], riverBoard) === 1,
);

const riverEq = poker.exactMultiwayEquityKnownHands([aa, kk, qq], riverBoard);
assertNear('river equities sum', sum(riverEq), 1, 1e-12);
assertTrue('AA wins this river', riverEq[0] === 1);

const wtl = poker.exactThreeWayWinTieLoseKnownHands(aa, kk, qq, riverBoard);
assertNear('river AA unique win', wtl.win[0], 1);
assertNear('river KK lose', wtl.lose[1], 1);
assertNear('river QQ lose', wtl.lose[2], 1);

const four = poker.exactFourWayEquityKnownHands(aa, kk, qq, ['Js', 'Jd'], riverBoard);
assertTrue('4-way length', four.length === 4);
assertNear('4-way sum', sum(four), 1, 1e-12);

const withDead = poker.exactMultiwayEquityWithDeadCards(
  [aa, kk, qq],
  ['2h', '7c', '9d'],
  ['3h', '3s'],
);
assertNear('dead-cards equity sum', sum(withDead), 1, 1e-9);
assertTrue(
  'dead cards shrink runouts',
  poker.exactMultiwayRunoutCount([aa, kk, qq], ['2h', '7c', '9d'], ['3h', '3s']) ===
    (41 * 40) / 2,
);

const side = poker.exactMultiwaySidePotChipEv(
  [10, 100, 100],
  [aa, kk, qq],
  riverBoard,
);
assertTrue('side pot two layers', side.layerCount === 2);
assertTrue('short stack EV <= 30', side.chipEv[0] <= 30 + 1e-9);
assertNear('side pot chips conserved', sum(side.chipEv), 210, 1e-9);

const flop = ['Ah', '7c', '3c'];
const ahead = poker.exactMultiwayAheadFrequency([aa, kk, qq], flop);
assertTrue('ahead now in [0,1]', ahead.pAheadNow >= 0 && ahead.pAheadNow <= 1);
assertTrue('showdown equity in [0,1]', ahead.pWinShowdown >= 0 && ahead.pWinShowdown <= 1);
assertTrue('AA ahead on Ace-high flop', ahead.pAheadNow === 1);

const ties = poker.exactMultiwayTieFrequency(
  [
    ['As', 'Kh'],
    ['Ad', 'Kc'],
    ['2c', '2d'],
  ],
  ['Ah', 'Kd', '7s', '3c', '9d'],
);
assertTrue('hero split on chopped river', ties.pHeroSplit === 1);
assertTrue('any split on chopped river', ties.pAnySplit === 1);

const bestWorst = poker.exactMultiwayBestWorstRunout([aa, kk, qq], flop);
assertTrue('best/worst supported on flop', bestWorst.supported === true);
assertTrue('best equity >= worst', bestWorst.bestEquity >= bestWorst.worstEquity);
assertTrue('best card present', typeof bestWorst.bestCard === 'string');

const preflopBw = poker.exactMultiwayBestWorstRunout([aa, kk, qq], []);
assertTrue('best/worst unsupported preflop', preflopBw.supported === false);

let twoWayThrew = false;
try {
  poker.exactMultiwayEquityKnownHands([aa, kk], []);
} catch {
  twoWayThrew = true;
}
assertTrue('n<3 rejected', twoWayThrew);

console.log('OK: verify-exact-multiway.mjs - all checks passed.');
