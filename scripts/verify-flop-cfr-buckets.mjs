/**
 * Runtime checks for bucketed flop CFR exports.
 * Run from NPM/: `node scripts/verify-flop-cfr-buckets.mjs` (requires built native addon).
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

function assertTrue(label, cond) {
  if (!cond) throw new Error(label);
}

function assertNear(label, actual, expected, tol) {
  if (Math.abs(actual - expected) > tol) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

const ranks = '23456789TJQKA';
const suits = 'cdhs';
function deckId(card) {
  const r = ranks.indexOf(card[0]);
  const s = suits.indexOf(card[1]);
  if (r < 0 || s < 0) throw new Error(`bad card ${card}`);
  return r * 4 + s;
}
function sparseHoles(pairs) {
  const indices = [];
  const weights = [];
  for (const [a, b, w] of pairs) {
    indices.push(deckId(a), deckId(b));
    weights.push(w ?? 1);
  }
  return { indices: Int32Array.from(indices), weights: Float64Array.from(weights) };
}
function comboIndex(a, b) {
  if (a > b) {
    const t = a;
    a = b;
    b = t;
  }
  return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

const names = [
  'ehs2BucketsVsRange',
  'bucketMassFromRange',  'flopBucketStrategyTo1326',
  'canonicalFlopCfrKey',  'flopBucketCountDefault',
];
for (const n of names) {
  assertTrue(`export ${n}`, typeof poker[n] === 'function');
}

const kDefault = poker.flopBucketCountDefault();
assertTrue(`default K is 10 or 20 (${kDefault})`, kDefault === 10 || kDefault === 20);

const flop = ['Ah', 'Kh', '2c'];
const villain = sparseHoles([['7d', '3s']]);
const k = 10;
const buckets = poker.ehs2BucketsVsRange(flop, villain, k);
assertTrue('ehs2BucketsVsRange length 1326', buckets.length === 1326);

const dead = new Set([deckId('Ah'), deckId('Kh'), deckId('2c')]);
let live = 0;
for (let a = 0; a < 52; a++) {
  for (let b = a + 1; b < 52; b++) {
    const idx = comboIndex(a, b);
    const blocked = dead.has(a) || dead.has(b);
    if (blocked) {
      assertTrue(`blocked combo ${idx} is -1`, buckets[idx] === -1);
    } else {
      assertTrue(`live bucket ${idx} in [0, ${k}) got ${buckets[idx]}`, buckets[idx] >= 0 && buckets[idx] < k);
      live += 1;
    }
  }
}
assertTrue('some live combos', live > 100);

const heroRange = sparseHoles([
  ['Ac', 'Ad'],
  ['Qc', 'Qd'],
  ['7c', '6d'],
]);
const masses = poker.bucketMassFromRange(heroRange, buckets, k);
assertTrue(`masses length ${masses.length}`, masses.length === k);
const massSum = Array.from(masses).reduce((s, x) => s + x, 0);
assertNear('bucket masses sum ~1', massSum, 1, 1e-9);

const mix = new Float64Array(k).fill(0.5);
const expanded = poker.flopBucketStrategyTo1326(mix, buckets);
assertTrue('expanded mix length 1326', expanded.length === 1326);
const aa = comboIndex(deckId('Ac'), deckId('Ad'));
if (buckets[aa] >= 0) {
  assertNear('AA inherits its bucket mix', expanded[aa], mix[buckets[aa]], 1e-12);
}

const keyH = poker.canonicalFlopCfrKey(['Ah', 'Kh', '9h'], 100, 200);
const keyD = poker.canonicalFlopCfrKey(['Ad', 'Kd', '9d'], 100, 200);
assertTrue(`canonical flop key stable under suit remap (${keyH})`, keyH === keyD);
assertTrue('key contains 1755', keyH.includes(String(poker.countCanonicalFlops())));
assertTrue('key contains flop index', keyH.includes(String(poker.isomorphicFlopIndex(['Ah', 'Kh', '9h']))));

console.log('OK: verify-flop-cfr-buckets.mjs - all checks passed.');
