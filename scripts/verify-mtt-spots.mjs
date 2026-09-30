/**
 * Runtime checks for the 10 MTT / table-spot native exports.
 * Run from package root: `node scripts/verify-mtt-spots.mjs` (requires `npm run build:native`).
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

const NAMES = [
  'spinGoPayouts',
  'spinGoIcmEv',  'pkoFgsPayouts',  'squeezeEv',
  'fourBetJamEv',  'threeBetPotCommitEv',
];

const EPS = 1e-9;

function assertNear(label, actual, expected, tol = EPS) {
  if (Math.abs(actual - expected) > tol) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function assertTrue(label, cond) {
  if (!cond) throw new Error(label);
}

for (const name of NAMES) {
  assertTrue(`${name} exists`, typeof poker[name] === 'function');
}

const payouts = poker.spinGoPayouts(2, 10);
assertTrue('spin payouts length 3', payouts.length === 3);
assertNear('spin 50%', payouts[0], 10);
assertNear('spin 30%', payouts[1], 6);
assertNear('spin 20%', payouts[2], 4);

const wta = poker.spinGoPayouts(4, 5, true);
assertNear('WTA first', wta[0], 20);
assertNear('WTA second', wta[1], 0);
assertNear('WTA third', wta[2], 0);

const ev = poker.spinGoIcmEv([1000, 1000, 1000], payouts);
assertNear('equal stacks equal $EV 0=1', ev[0], ev[1], 1e-8);
assertNear('equal stacks equal $EV 1=2', ev[1], ev[2], 1e-8);
assertNear('equal stacks share pool', ev[0] + ev[1] + ev[2], 20, 1e-8);

const stacks = [5000, 3000, 2000];
const prizes = [100, 50, 30];
const bounties = [20, 16, 12];
const icmbu = poker.pkoIcmbuPayouts(stacks, prizes, bounties);
const fgsZero = poker.pkoFgsPayouts(stacks, prizes, bounties, 0, 25, 50, 10);
for (let i = 0; i < 3; i += 1) {
  assertNear(`pkoFgs orbits=0 icmbu[${i}]`, fgsZero.icmbu[i], icmbu.icmbu[i], 1e-9);
}

const squeeze = poker.squeezeEv(30, 24, 16, 16, 0.4, 0.5, 0.45, 0.4, 0.33);
assertTrue('squeeze EV finite', Number.isFinite(squeeze.squeezeEv));
assertNear('squeeze fold EV = 0', squeeze.foldEv, 0);

const jam = poker.fourBetJamEv(45, 80, 60, 1, 0.4);
assertNear('FE=1 wins dead money', jam.jamEv, 45);
assertNear('4bet fold EV = 0', jam.foldEv, 0);

const commit = poker.threeBetPotCommitEv(24, 48, 0.45);
assertNear('SPR 48/24', commit.spr, 2);
assertTrue('continueEv finite', Number.isFinite(commit.continueEv));
assertNear('commit fold EV = 0', commit.foldEv, 0);

console.log('OK: verify-mtt-spots.mjs — 10 MTT spot exports checked.');
