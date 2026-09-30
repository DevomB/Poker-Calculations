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

const names = [  'icmPayoutsAfterBlindPost',  'icmCallingBubbleFactor',
  'fgsPayoutsBlindSchedule',  'icmDeadPotDollarEv',
];
for (const name of names) {
  assertTrue(`${name} is a function`, typeof poker[name] === 'function');
}

const stacks = [5000, 3000, 2000];
const payouts = [100, 60, 40];
const icm = Array.from(poker.icmExpectedPayouts(stacks, payouts));

const zeroPost = Array.from(poker.icmPayoutsAfterBlindPost(stacks, payouts, 0, 0, [0, 0, 0]));
for (let i = 0; i < 3; ++i) {
  assertNear(`zero post equals ICM seat ${i}`, zeroPost[i], icm[i], EPS);
}
// A seat blinded to zero keeps its finishing prize (bottom payout), so the pool is conserved.
const busted = Array.from(poker.icmPayoutsAfterBlindPost([5000, 3000, 100], payouts, 2, 100, [0, 0, 100]));
assertNear('busted seat keeps the bottom prize', busted[2], payouts[2], EPS);
assertNear('prize pool conserved after a bust', busted[0] + busted[1] + busted[2], 200, EPS);

const bubbleStacks = [4000, 2500, 1500];
const bubblePayouts = [100, 50, 0];
const callingBf = poker.icmCallingBubbleFactor(bubbleStacks, bubblePayouts, 1, 2, 1500);
assertTrue(`calling bubble factor > 1 (got ${callingBf})`, Number.isFinite(callingBf) && callingBf > 1);

const dead = poker.icmDeadPotDollarEv([5000, 2000, 1000], [100, 50, 0], 0, 500);
assertTrue('dead-pot winEv > nowEv', dead.winEv > dead.nowEv);
assertTrue('dead-pot delta positive', dead.delta > 0);

const posted = Array.from(poker.icmPayoutsAfterBlindPost(stacks, payouts, 0, 50, [50, 25, 10]));
assertTrue('after-post length', posted.length === 3);
assertTrue('after-post finite', posted.every(Number.isFinite));

const sched = Array.from(
  poker.fgsPayoutsBlindSchedule(stacks, payouts, [25, 50], [50, 100], [0, 10], [1, 1]),
);
assertTrue('schedule length', sched.length === 3);

console.log('OK: verify-fgs.mjs — all checks passed.');
