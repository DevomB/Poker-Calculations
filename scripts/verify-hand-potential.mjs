/**
 * Hand potential (HS / PPot / NPot / EHS / EHS2) checks.
 * Run from NPM/: `node scripts/verify-hand-potential.mjs` (requires built native addon).
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

function comboIndex(a, b) {
  if (a > b) {
    const t = a;
    a = b;
    b = t;
  }
  return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

function packedIds(text) {
  return Array.from(poker.parseCompactCardList(text, { outFormat: 'packed' }));
}

function sparse(holes) {
  const indices = [];
  const weights = [];
  for (const h of holes) {
    const ids = packedIds(h);
    indices.push(ids[0], ids[1]);
    weights.push(1);
  }
  return { indices, weights };
}

const names = [
  'handStrengthVsRange',
  'positivePotentialVsRange',
  'negativePotentialVsRange',
  'effectiveHandStrength',
  'effectiveHandStrengthSquared',
  'handPotentialBreakdown',
  'twoStreetPositivePotential',
  'twoStreetNegativePotential',
  'equityBucketFromEhs',
  'comboEhsTableVsRange',
];
for (const name of names) {
  assertTrue(`${name} exported`, typeof poker[name] === 'function');
}

const exportCount = Object.keys(poker).filter((k) => typeof poker[k] === 'function').length;
assertTrue(`native export count >= 360 (got ${exportCount})`, exportCount >= 360);

// Nuts on a wet flop: royal on Qh Jh Th. HS ~1, PPot ~0, NPot low (stays nuts).
const royal = ['Ah', 'Kh'];
const wetNuts = ['Qh', 'Jh', 'Th'];
const airRange = sparse(['9c8d']);
const nuts = poker.handPotentialBreakdown(royal, wetNuts, airRange);
assertTrue(`nuts HS ~1 (got ${nuts.hs})`, nuts.hs > 0.999);
assertTrue(`nuts PPot ~0 (got ${nuts.ppot})`, nuts.ppot < 1e-9);
assertTrue(`nuts NPot low-ish (got ${nuts.npot})`, nuts.npot < 0.05);
assertTrue(`nuts EHS in [0,1] (${nuts.ehs})`, nuts.ehs >= 0 && nuts.ehs <= 1);
assertTrue(`nuts EHS2 in [0,1] (${nuts.ehs2})`, nuts.ehs2 >= 0 && nuts.ehs2 <= 1);

// Air with flush draw vs a pair: HS low, PPot clearly > 0.
const fdHero = ['7h', '6h'];
const fdBoard = ['Ah', 'Kh', '2c'];
const pairRange = sparse(['9d9s']);
const draw = poker.handPotentialBreakdown(fdHero, fdBoard, pairRange);
assertTrue(`draw HS low (got ${draw.hs})`, draw.hs < 0.15);
assertTrue(`draw PPot > 0 (got ${draw.ppot})`, draw.ppot > 0.1);
assertTrue(`draw EHS in [0,1] (${draw.ehs})`, draw.ehs >= 0 && draw.ehs <= 1);

// Tiny range: breakdown matches individual calls.
const tinyHero = ['As', 'Kd'];
const tinyBoard = ['2h', '7c', '9s'];
const tinyRange = sparse(['QcJd', '5h5d']);
const br = poker.handPotentialBreakdown(tinyHero, tinyBoard, tinyRange);
assertNear('breakdown hs', br.hs, poker.handStrengthVsRange(tinyHero, tinyBoard, tinyRange), 1e-6);
assertNear('breakdown ppot', br.ppot, poker.positivePotentialVsRange(tinyHero, tinyBoard, tinyRange), 1e-6);
assertNear('breakdown npot', br.npot, poker.negativePotentialVsRange(tinyHero, tinyBoard, tinyRange), 1e-6);
assertNear('breakdown ehs', br.ehs, poker.effectiveHandStrength(tinyHero, tinyBoard, tinyRange), 1e-6);
assertNear('breakdown ehs2', br.ehs2, poker.effectiveHandStrengthSquared(tinyHero, tinyBoard, tinyRange), 1e-6);

const ehs = br.ehs;
assertTrue('EHS formula', Math.abs(ehs - (br.hs * (1 - br.npot) + (1 - br.hs) * br.ppot)) < 1e-12);

assertTrue('two-street PPot in [0,1]', (() => {
  const p = poker.twoStreetPositivePotential(fdHero, fdBoard, pairRange);
  return p >= 0 && p <= 1 && p > draw.ppot - 1e-9;
})());
assertTrue('two-street NPot in [0,1]', (() => {
  const n = poker.twoStreetNegativePotential(royal, wetNuts, airRange);
  return n >= 0 && n <= 1 && n < 0.05;
})());

assertTrue('bucket 0', poker.equityBucketFromEhs(0, 10) === 0);
assertTrue('bucket last', poker.equityBucketFromEhs(1, 10) === 9);
assertTrue('bucket mid', poker.equityBucketFromEhs(0.55, 10) === 5);

const table = poker.comboEhsTableVsRange(tinyBoard, tinyRange);
assertTrue('combo table length 1326', table.length === 1326);
const heroIds = packedIds('AsKd');
const heroIdx = comboIndex(heroIds[0], heroIds[1]);
assertNear('combo table matches EHS', table[heroIdx], br.ehs, 1e-6);
const boardIds = packedIds('2h7c9s');
for (const id of boardIds) {
  for (let other = 0; other < 52; other++) {
    if (other === id || boardIds.includes(other)) continue;
    const idx = comboIndex(id, other);
    if (table[idx] !== 0) {
      throw new Error(`blocked combo ${idx} should be 0, got ${table[idx]}`);
    }
  }
}

console.log('OK: verify-hand-potential.mjs - all checks passed.');
