/**
 * Short Deck (6+) native exports. Run from NPM/: `node scripts/verify-short-deck.mjs`
 * (requires built native addon).
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
  try {
    fn();
  } catch {
    return;
  }
  throw new Error(`${label}: expected throw`);
}

const names = [
  'evaluateShortDeckBestHand',
  'evaluateShortDeckHandStrength',
  'evaluateShortDeckCategory',
  'exactHuShortDeckEquityVsKnown',
  'simulateShortDeckEquityVsRandom',
  'simulateShortDeckEquityVsRange',
  'shortDeckStraightIsWheel',
  'shortDeckRemainingComboCount',
  'shortDeckNashHuJamRange',
  'shortDeckVsHoldemCategoryFlip',
];
for (const n of names) {
  assertTrue(`export ${n}`, typeof poker[n] === 'function');
}

const exportCount = Object.keys(poker).filter((k) => typeof poker[k] === 'function').length;
assertTrue(`runtime export count >= 360 (got ${exportCount})`, exportCount >= 360);

const wheelSuited = poker.evaluateShortDeckBestHand(['As', '6s', '7s', '8s', '9s']);
assertTrue(
  `A6789 suited is wheel SF not high card (got ${wheelSuited.rank})`,
  wheelSuited.rank === 'straightFlush' && wheelSuited.rank !== 'highCard' && wheelSuited.rank !== 'flush',
);
assertTrue('suited A6789 is wheel', poker.shortDeckStraightIsWheel(['As', '6s', '7s', '8s', '9s']));

const wheelOff = poker.evaluateShortDeckBestHand(['Ad', '6c', '7h', '8s', '9d']);
assertTrue(`A6789 offsuit is straight (got ${wheelOff.rank})`, wheelOff.rank === 'straight');
assertTrue('offsuit A6789 is wheel', poker.shortDeckStraightIsWheel(['Ad', '6c', '7h', '8s', '9d']));
assertTrue('A6789 flips vs Hold\'em', poker.shortDeckVsHoldemCategoryFlip(['Ad', '6c', '7h', '8s', '9d']));

const broadway = poker.evaluateShortDeckBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th']);
assertTrue('AKQJT suited is royal', broadway.rank === 'royalFlush');

const flushS = poker.evaluateShortDeckHandStrength(['Ah', 'Kh'], ['9h', '8h', '7h']);
const boatS = poker.evaluateShortDeckHandStrength(['6c', '6d'], ['6s', '9c', '9d']);
assertTrue(`6+ flush beats boat (${flushS} vs ${boatS})`, flushS > boatS);
assertTrue(
  'NLHE boat still beats flush',
  poker.evaluateHandStrength(['6c', '6d'], ['6s', '9c', '9d']) >
    poker.evaluateHandStrength(['Ah', 'Kh'], ['9h', '8h', '7h']),
);
assertTrue(
  'flush category ordinal 6 > boat 5',
  poker.evaluateShortDeckBestHand(['Ah', 'Kh', '9h', '8h', '7h']).rankCategory === 6 &&
    poker.evaluateShortDeckBestHand(['6c', '6d', '6s', '9c', '9d']).rankCategory === 5,
);
assertTrue(
  'plain flush does not flip category label vs Hold\'em',
  poker.shortDeckVsHoldemCategoryFlip(['Ah', 'Kh', '9h', '8h', '7h']) === false,
);

assertThrows('reject 2', () => poker.evaluateShortDeckBestHand(['Ah', '2c', 'Kd', 'Qs', 'Jh']));
assertThrows('reject 5', () => poker.evaluateShortDeckBestHand(['Ah', '5d', 'Kd', 'Qs', 'Jh']));
assertThrows('reject 2 in equity', () =>
  poker.exactHuShortDeckEquityVsKnown(['Ah', 'Kd'], ['2c', '2d'], ['Qs', 'Js', 'Ts']),
);

const eq = poker.exactHuShortDeckEquityVsKnown(['Ah', 'Kd'], ['6c', '6d'], ['Qs', 'Js', 'Ts']);
const eqFlip = poker.exactHuShortDeckEquityVsKnown(['6c', '6d'], ['Ah', 'Kd'], ['Qs', 'Js', 'Ts']);
assertNear('exact HU equities sum to 1', eq + eqFlip, 1, 1e-12);
assertTrue('exact HU in (0,1)', eq > 0 && eq < 1);

assertNear('empty dead C(36,2)', poker.shortDeckRemainingComboCount([]), 630);
assertNear('4 dead C(32,2)', poker.shortDeckRemainingComboCount(['Ah', 'Kd', '6c', '6d']), 496);

const vsRand = poker.simulateShortDeckEquityVsRandom(['Ah', 'Ad'], [], 400, 1);
assertTrue(`AA vs random 6+ in (0.5,1) got ${vsRand}`, vsRand > 0.5 && vsRand < 1);
const vsRange = poker.simulateShortDeckEquityVsRange(['Ah', 'Ad'], [], ['QQ', 'AKs', '76s'], 400, 2);
assertTrue(`AA vs range in (0,1) got ${vsRange}`, vsRange > 0 && vsRange < 1);

const jam = poker.shortDeckNashHuJamRange({
  stackBb: 8,
  equityIterations: 4,
  maxIterations: 8,
  equitySeed: 1,
});
assertTrue('nash jam length 81', jam.length === 81);
assertTrue(
  'nash jam freqs in [0,1]',
  Array.from(jam).every((x) => x >= 0 && x <= 1),
);

console.log('OK: verify-short-deck.mjs - all checks passed.');
