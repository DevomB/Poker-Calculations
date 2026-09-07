/**
 * Runtime checks for the 10 PKO / bounty native exports.
 * Run from package root: `node scripts/verify-pko.mjs` (requires `npm run build:native`).
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
  'pkoKnockoutProbabilityMatrix',
  'pkoExpectedBountyCollection',
  'pkoIcmbuPayouts',
  'pkoBountyRiskPremium',
  'pkoCallEvVsShove',
  'pkoJamEvVsFold',
  'mysteryBountyExpectedValue',
  'progressiveKoPostedBounty',
  'pkoCoveringHuntEv',
  'pkoWinnerTakeRemainingBounties',
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

const stacks = [5000, 3000, 2000];
const payouts = [100, 50, 30];
const bounties = [20, 16, 12];

const ko = poker.pkoKnockoutProbabilityMatrix(stacks);
assertTrue('knockout n', ko.n === 3);
assertTrue('knockout Float64Array', ko.matrix instanceof Float64Array && ko.matrix.length === 9);
for (let i = 0; i < 3; i += 1) {
  assertNear(`knockout diagonal ${i}`, ko.matrix[i * 3 + i], 0);
}

const icm = poker.icmExpectedPayouts(stacks, payouts);
const icmbu = poker.pkoIcmbuPayouts(stacks, payouts, bounties);
for (let i = 0; i < 3; i += 1) {
  assertTrue(`ICMBU[${i}] >= ICM[${i}]`, icmbu.icmbu[i] >= icm[i] - 1e-9);
  assertNear(`ICMBU icm matches freezeout ${i}`, icmbu.icm[i], icm[i], 1e-9);
}

const zeroBounty = poker.pkoIcmbuPayouts(stacks, payouts, [0, 0, 0]);
for (let i = 0; i < 3; i += 1) {
  assertNear(`zero bounty ICMBU=${i} matches ICM`, zeroBounty.icmbu[i], icm[i], 1e-9);
}

const mystery = poker.mysteryBountyExpectedValue([10, 20, 30]);
assertNear('mystery equal-weight mean', mystery.oneDraw, 20);
assertNear('mystery all remaining', mystery.allRemaining, 60);

const mysteryK = poker.mysteryBountyExpectedValue([10, 20, 30], undefined, 2);
assertNear('mystery sample k=2', mysteryK.sampleK, 40);

const hero = 0;
const villain = 1;
const pot = 150;
const spot = poker.pkoCallEvVsShove(stacks, payouts, bounties, hero, villain, pot, 0.55);
const bountyEv = poker.pkoExpectedBountyCollection(stacks, bounties);
assertNear('call fold path ICM+bounty', spot.foldEv, icm[hero] + bountyEv[hero], 1e-8);
assertTrue('callEv finite', Number.isFinite(spot.callEv));

const huStacks = [100, 100];
const huPay = [150, 50];
const huBounty = [25, 25];
const huKo = poker.pkoKnockoutProbabilityMatrix(huStacks);
assertNear('HU P(0 knocks 1)', huKo.matrix[1], 0.5, 1e-9);
assertNear('HU P(1 knocks 0)', huKo.matrix[2], 0.5, 1e-9);

const posted = poker.progressiveKoPostedBounty([10, 10, 10], [0, 10, 0], 1);
assertNear('progressive carry', posted[1], 20);

const hunt = poker.pkoCoveringHuntEv(stacks, payouts, bounties, 0, 2, 50);
assertTrue('hunt covers short', Number.isFinite(hunt.huntEv) && Number.isFinite(hunt.checkDownEv));

const leftover = poker.pkoWinnerTakeRemainingBounties(stacks, payouts, 40);
assertNear('winner-take first prize', leftover.adjustedPayouts[0], 140);
assertNear(
  'winner-take bounty term sum',
  leftover.bountyToWinnerEv.reduce((a, b) => a + b, 0),
  40,
  1e-8,
);

const jam = poker.pkoJamEvVsFold(stacks, payouts, bounties, 0, 1, 150, 0.3, 0.5);
assertTrue('jamEv finite', Number.isFinite(jam.jamEv) && Number.isFinite(jam.foldEv));

const premium = poker.pkoBountyRiskPremium(stacks, payouts, bounties);
assertNear('risk premium freezeout[0]', premium.freezeoutIcm[0], icm[0], 1e-9);

console.log('OK: verify-pko.mjs — 10 PKO exports checked.');
