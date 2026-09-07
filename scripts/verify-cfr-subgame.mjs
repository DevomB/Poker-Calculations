/**
 * Runtime checks for CFR / best-response subgame exports.
 * Run from NPM/: `node scripts/verify-cfr-subgame.mjs` (requires built native addon).
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

// --- regret matching ---
const rm = poker.regretMatchingStrategy([1, -1, 0]);
assertNear('regretMatching [1,-1,0][0]', rm[0], 1);
assertNear('regretMatching [1,-1,0][1]', rm[1], 0);
assertNear('regretMatching [1,-1,0][2]', rm[2], 0);

const uniform = poker.regretMatchingStrategy([-2, -1, -0.5]);
assertNear('all-negative uniform 0', uniform[0], 1 / 3, 1e-12);
assertNear('all-negative uniform 1', uniform[1], 1 / 3, 1e-12);
assertNear('all-negative uniform 2', uniform[2], 1 / 3, 1e-12);

// --- RPS via cfrNodeReachUpdate ---
const pay = [
  [0, -1, 1],
  [1, 0, -1],
  [-1, 1, 0],
];
let r0 = [0, 0, 0];
let r1 = [0, 0, 0];
for (let t = 0; t < 8000; t++) {
  const s0 = poker.regretMatchingStrategy(r0);
  const s1 = poker.regretMatchingStrategy(r1);
  const v0 = [0, 0, 0];
  const v1 = [0, 0, 0];
  for (let a = 0; a < 3; a++) {
    for (let b = 0; b < 3; b++) {
      v0[a] += s1[b] * pay[a][b];
      v1[b] += s0[a] * -pay[a][b];
    }
  }
  const ev0 = v0[0] * s0[0] + v0[1] * s0[1] + v0[2] * s0[2];
  const ev1 = v1[0] * s1[0] + v1[1] * s1[1] + v1[2] * s1[2];
  const u0 = poker.cfrNodeReachUpdate(r0, [v0[0] - ev0, v0[1] - ev0, v0[2] - ev0], 1);
  const u1 = poker.cfrNodeReachUpdate(r1, [v1[0] - ev1, v1[1] - ev1, v1[2] - ev1], 1);
  r0 = Array.from(u0.regrets);
  r1 = Array.from(u1.regrets);
}
const rps = poker.regretMatchingStrategy(r0);
assertNear('RPS a0', rps[0], 1 / 3, 0.04);
assertNear('RPS a1', rps[1], 1 / 3, 0.04);
assertNear('RPS a2', rps[2], 1 / 3, 0.04);

const support = poker.strategySupportSize([0, 0.5, 1], 0.02);
assertTrue('strategySupportSize mixed', support.mixedCount === 1);
assertTrue('strategySupportSize pureMass > 0', support.pureMass > 0);

// --- river: nuts bettor vs air defender ---
const board = ['Ah', 'Kh', 'Qh', 'Jh', '2c'];
const nuts = sparseHoles([['Th', '9d']]);
const air = sparseHoles([['4c', '5d']]);
const river = poker.cfrRiverBetCallFoldSolve(100, 50, nuts, air, board, 400);
assertTrue(`nuts bet freq high (${river.betFreq})`, river.betFreq > 0.85);
assertTrue(`air call freq low (${river.callFreq})`, river.callFreq < 0.15);
assertTrue('river ev finite', Number.isFinite(river.evBettor) && Number.isFinite(river.evDefender));

const expl = poker.exploitabilityRiver(100, 50, nuts, air, board, river.betMix, river.callMix);
assertTrue(`exploitability >= -1e-9 (${expl})`, expl >= -1e-9);

const ev = poker.evOfStrategyProfile(100, 50, nuts, air, board, river.betMix, river.callMix);
assertNear('ev profile matches solve bettor', ev.evBettor, river.evBettor, 1e-6);

const br = poker.bestResponseRiver(100, 50, air, nuts, board, river.betMix);
assertTrue('BR action is fold or call', br.action === 'fold' || br.action === 'call');
assertTrue('BR vs nuts mostly fold', br.callFrequency < 0.2);

const fp = poker.fictitiousPlayRiver(100, 50, nuts, air, board, 200);
assertTrue('fictitious play bet high', fp.betFreq > 0.8);

const tree = poker.solveHuRiverCheckBetTree(100, 50, nuts, air, board, 200, 3);
assertTrue('topBetCombos non-empty', tree.topBetCombos.length >= 1);
assertTrue('classes object', typeof tree.classes.strongBet === 'number');

// --- push-fold: AA jam mass high ---
const aa = sparseHoles([
  ['Ac', 'Ad'],
  ['Ac', 'Ah'],
  ['Ac', 'As'],
  ['Ad', 'Ah'],
  ['Ad', 'As'],
  ['Ah', 'As'],
]);
const trash = sparseHoles([
  ['7c', '2d'],
  ['7c', '2h'],
  ['7d', '2c'],
  ['7h', '2s'],
]);
const pf = poker.cfrHeadsUpPushFoldSolve(aa, trash, 10, 200);
assertTrue(`AA jam mass high (${pf.jamFreq})`, pf.jamFreq > 0.8);

const names = [
  'regretMatchingStrategy',
  'cfrRiverBetCallFoldSolve',
  'bestResponseRiver',
  'exploitabilityRiver',
  'cfrHeadsUpPushFoldSolve',
  'fictitiousPlayRiver',
  'evOfStrategyProfile',
  'strategySupportSize',
  'cfrNodeReachUpdate',
  'solveHuRiverCheckBetTree',
];
for (const n of names) {
  assertTrue(`export ${n}`, typeof poker[n] === 'function');
}

console.log('OK: verify-cfr-subgame.mjs - all checks passed.');
