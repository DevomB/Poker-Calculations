/**
 * Runtime checks for Future Game Simulation and ICM decision exports.
 * Run from NPM/: `node scripts/verify-fgs.mjs` (requires built native addon).
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
function assertNear(label, actual, expected, tol = 1e-9) {
  if (!Number.isFinite(actual) || Math.abs(actual - expected) > tol) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}
function assertTrue(label, cond) {
  if (!cond) throw new Error(label);
}

const names = [
  'futureGameSimulationPayouts',
  'futureGrowthShare',
  'icmPayoutsAfterBlindPost',
  'icmJamVsFoldEv',
  'icmCallVsFoldEv',
  'icmCallingBubbleFactor',
  'fgsPayoutsBlindSchedule',
  'icmStallingEv',
  'icmPayJumpSurvivalEv',
  'icmDeadPotDollarEv',
];
for (const name of names) {
  assertTrue(`${name} is a function`, typeof poker[name] === 'function');
}

const stacks = [5000, 3000, 2000];
const payouts = [100, 60, 40];
const icm = Array.from(poker.icmExpectedPayouts(stacks, payouts));

const fgsZeroOrbits = Array.from(poker.futureGameSimulationPayouts(stacks, payouts, 0, 25, 50, 10));
const fgsZeroCost = Array.from(poker.futureGameSimulationPayouts(stacks, payouts, 3, 0, 0, 0));
for (let i = 0; i < 3; ++i) {
  assertNear(`FGS orbits=0 seat ${i}`, fgsZeroOrbits[i], icm[i], EPS);
  assertNear(`FGS zero cost seat ${i}`, fgsZeroCost[i], icm[i], EPS);
}

const bubbleStacks = [4000, 2500, 1500];
const bubblePayouts = [100, 50, 0];
const callingBf = poker.icmCallingBubbleFactor(bubbleStacks, bubblePayouts, 1, 2, 1500);
assertTrue(`calling bubble factor > 1 (got ${callingBf})`, Number.isFinite(callingBf) && callingBf > 1);

const jam = poker.icmJamVsFoldEv(stacks, payouts, 0, 1, 150, 0.3, 0.55);
assertTrue('jam foldEv finite', Number.isFinite(jam.foldEv));
assertTrue('jam jamEv finite', Number.isFinite(jam.jamEv));
assertTrue('jam delta finite', Number.isFinite(jam.delta));

const dead = poker.icmDeadPotDollarEv([5000, 2000, 1000], [100, 50, 0], 0, 500);
assertTrue('dead-pot winEv > nowEv', dead.winEv > dead.nowEv);
assertTrue('dead-pot delta positive', dead.delta > 0);

const growth = poker.futureGrowthShare([4000, 800, 800], 3, 25, 50, 0);
assertTrue('growth survivorCount >= 1', growth.survivorCount >= 1);
assertTrue('growth net length', growth.netGrowth.length === 3);

const posted = Array.from(poker.icmPayoutsAfterBlindPost(stacks, payouts, 0, 50, [50, 25, 10]));
assertTrue('after-post length', posted.length === 3);
assertTrue('after-post finite', posted.every(Number.isFinite));

const call = poker.icmCallVsFoldEv(stacks, payouts, 1, 0, 200, 300, 0.42);
assertTrue('call delta finite', Number.isFinite(call.delta));

const sched = Array.from(
  poker.fgsPayoutsBlindSchedule(stacks, payouts, [25, 50], [50, 100], [0, 10], [1, 1]),
);
assertTrue('schedule length', sched.length === 3);

const stall = poker.icmStallingEv(stacks, payouts, 2, 25, 50, 0);
assertTrue('stall premium finite', Number.isFinite(stall.stallingPremium));

const jump = poker.icmPayJumpSurvivalEv(stacks, payouts, 0, { bustChips: 'vanish' });
assertTrue('pay-jump ladder finite', Number.isFinite(jump.ladderDelta));

console.log('OK: verify-fgs.mjs — all checks passed.');
