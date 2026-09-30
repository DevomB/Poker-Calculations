/**
 * 7-card stud hi + razz (A-5 low) export checks.
 * Run from NPM/: `node scripts/verify-stud-razz.mjs` (requires built native addon).
 *
 * Razz ≠ 2-7: aces low, straights/flushes do not count, wheel is A2345.
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

const names = [
  'evaluateStudBestHand',
  'evaluateRazzHand',
  'razzWheelIsNuts',
  'exactHuStudEquityVsKnown',
  'exactHuRazzEquityVsKnown',  'studDeadCardDeck',
  'simulateStudEquityVsRandom',
  'simulateRazzEquityVsRandom',
];
for (const n of names) {
  assertTrue(`${n} exported`, typeof poker[n] === 'function');
}

const wheel = ['As', '2h', '3c', '4d', '5s'];
const sixHigh = ['As', '2h', '3c', '4d', '6s'];
const wheelEv = poker.evaluateRazzHand(wheel);
const sixEv = poker.evaluateRazzHand(sixHigh);
assertTrue('wheel is highCard (flush/straight ignored)', wheelEv.rank === 'highCard');
assertTrue('A2345 razz beats A2346', wheelEv.strength < sixEv.strength);
assertTrue('wheel is nuts', poker.razzWheelIsNuts(wheel) === true);
assertTrue('A2346 is not the wheel', poker.razzWheelIsNuts(sixHigh) === false);
assertTrue(
  'suited wheel still nuts',
  poker.razzWheelIsNuts(['Ah', '2h', '3h', '4h', '5h']) === true,
);

const royal = ['Ah', 'Kh', 'Qh', 'Jh', 'Th', '2c', '3d'];
const quads = ['Ac', 'Ad', '9c', '9d', '9h', '9s', '2s'];
const royalEv = poker.evaluateStudBestHand(royal);
const quadsEv = poker.evaluateStudBestHand(quads);
assertTrue('7-card royal', royalEv.rank === 'royalFlush');
assertTrue('quads nines', quadsEv.rank === 'fourOfAKind');
assertTrue('7-card royal stud beats quads', royalEv.strength > quadsEv.strength);

const eqRoyal = poker.exactHuStudEquityVsKnown(royal, quads);
const eqQuads = poker.exactHuStudEquityVsKnown(quads, royal);
assertNear('royal scoops vs quads', eqRoyal, 1);
assertNear('quads lose to royal', eqQuads, 0);
assertNear('HU stud equities sum 1 when both 7 known', eqRoyal + eqQuads, 1);

const razzWheel7 = ['As', '2h', '3c', '4d', '5s', 'Kh', 'Kd'];
const razzSix7 = ['Ac', '2c', '3d', '4h', '6s', 'Qs', 'Qc'];
const eqWheel = poker.exactHuRazzEquityVsKnown(razzWheel7, razzSix7);
const eqSix = poker.exactHuRazzEquityVsKnown(razzSix7, razzWheel7);
assertNear('wheel scoops vs 6-high razz', eqWheel, 1);
assertNear('6-high loses to wheel', eqSix, 0);
assertNear('HU razz equities sum 1 when both 7 known', eqWheel + eqSix, 1);

const ranks = '23456789TJQKA';
const suits = 'cdhs';
const full = [];
for (const r of ranks) {
  for (const s of suits) {
    full.push(`${r}${s}`);
  }
}
assertTrue('full deck remainder empty', poker.studDeadCardDeck(full).length === 0);

const live = poker.studDeadCardDeck(['Ah', 'Kd']);
assertTrue('dead deck drops 2', live.length === 50);
assertTrue('Ah removed', !live.includes('Ah'));

const studMc = poker.simulateStudEquityVsRandom(royal, 40, 1);
assertTrue('stud MC in [0,1]', studMc >= 0 && studMc <= 1);
const razzMc = poker.simulateRazzEquityVsRandom(razzWheel7, 40, 2);
assertTrue('razz MC in [0,1]', razzMc >= 0 && razzMc <= 1);

const n = Object.keys(poker).filter((k) => typeof poker[k] === 'function').length;
assertTrue(`smoke native count >= 360 (got ${n})`, n >= 360);

console.log('OK: verify-stud-razz.mjs - all checks passed.');
