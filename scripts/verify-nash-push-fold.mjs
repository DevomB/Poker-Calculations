/**
 * Numeric checks for Nash jam/fold exports.
 * Run from NPM/: `node scripts/verify-nash-push-fold.mjs` (requires `npm run build:native`).
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

function assertTrue(label, cond) {
  if (!cond) {
    throw new Error(label);
  }
}

function jamCount(freq, thresh = 0.5) {
  let n = 0;
  for (let i = 0; i < freq.length; i += 1) {
    if (freq[i] >= thresh) n += 1;
  }
  return n;
}

const aa = 168;
const o72 = 10;

const names = [
  'nashHeadsUpJamRange',
  'nashHeadsUpCallRange',
  'nashHeadsUpJamCallSolve',
  'nashBlindVsBlindSolve',
  'nashFirstInJamRange',
  'nashJamFoldChart169',
  'nashCallChart169',
  'nashIndifferenceStackBb',
  'nashIcmHeadsUpJamCallSolve',
  'nashMultiwayShoveCall',
];
for (const name of names) {
  assertTrue(`${name} exported`, typeof poker[name] === 'function');
}

const jam10 = poker.nashHeadsUpJamRange(10, 0.5, 1, 0);
assertTrue('jam10 length 169', jam10.length === 169);
assertTrue(`AA jam at 10bb ~1 (got ${jam10[aa]})`, jam10[aa] >= 0.9);
assertTrue(`72o jam at 10bb ~0 (got ${jam10[o72]})`, jam10[o72] <= 0.1);

const jam3 = poker.nashHeadsUpJamRange(3, 0.5, 1, 0);
const jam12 = poker.nashHeadsUpJamRange(12, 0.5, 1, 0);
assertTrue(
  `shorter stack jams more (3bb=${jamCount(jam3)} 12bb=${jamCount(jam12)})`,
  jamCount(jam3) > jamCount(jam12),
);

const mass8 = jamCount(poker.nashHeadsUpJamRange(8, 0.5, 1, 0));
const mass10 = jamCount(jam10);
const mass12 = jamCount(jam12);
assertTrue(`8bb jam mass in band (${mass8})`, mass8 > 0 && mass8 < 169);
assertTrue(`10bb jam mass in band (${mass10})`, mass10 > 0 && mass10 < 169);
assertTrue(`12bb jam mass in band (${mass12})`, mass12 > 0 && mass12 < 169);

const call10 = poker.nashHeadsUpCallRange(10, 0.5, 1, 0);
assertTrue('call10 length 169', call10.length === 169);
assertTrue(`AA calls a jam at 10bb (got ${call10[aa]})`, call10[aa] >= 0.9);

const solved = poker.nashHeadsUpJamCallSolve(10, 0.5, 1, 0);
assertTrue('solve jam/call length', solved.jam.length === 169 && solved.call.length === 169);
assertTrue('solve iterations in 1..80', solved.iterations >= 1 && solved.iterations <= 80);
assertTrue('solve heroEv finite', Number.isFinite(solved.heroEv));
assertTrue('solve villainEv finite', Number.isFinite(solved.villainEv));

const bvb = poker.nashBlindVsBlindSolve(10, 0.5, 1, 0);
assertTrue('bvb AA jams', bvb.jam[aa] >= 0.9);
assertTrue('bvb 72o folds', bvb.jam[o72] <= 0.2);

const firstIn = poker.nashFirstInJamRange({ stackBb: 10, nOpponents: 2, stacks: [10, 12] });
assertTrue('first-in length 169', firstIn.length === 169);
assertTrue('first-in AA jams', firstIn[aa] >= 0.9);

const jamChart = poker.nashJamFoldChart169({ bigBlind: 1, ante: 0, maxStackBb: 16 });
const callChart = poker.nashCallChart169({ bigBlind: 1, ante: 0, maxStackBb: 16 });
assertTrue('jam chart length 169', jamChart.length === 169);
assertTrue('call chart length 169', callChart.length === 169);
assertTrue(
  `AA jam threshold > 72o (${jamChart[aa]} vs ${jamChart[o72]})`,
  jamChart[aa] > jamChart[o72],
);

const indAa = poker.nashIndifferenceStackBb('AA', { maxStackBb: 16, smallBlind: 0.5, bigBlind: 1 });
const ind72 = poker.nashIndifferenceStackBb('72o', { maxStackBb: 16, smallBlind: 0.5, bigBlind: 1 });
assertTrue(`AA indifference > 72o (${indAa} vs ${ind72})`, indAa > ind72);
assertTrue('72o indifference is short', ind72 < 8);

const chipOpts = { heroStack: 10, villainStack: 20, smallBlind: 0.5, bigBlind: 1, ante: 0 };
const chip = poker.nashHeadsUpJamCallSolve(chipOpts);
const icm = poker.nashIcmHeadsUpJamCallSolve({
  ...chipOpts,
  otherStacks: [40],
  payouts: [70, 30, 0],
});
const chipMass = jamCount(chip.jam);
const icmMass = jamCount(icm.jam);
assertTrue(
  `ICM short-stack jams tighter than chip on bubble (${icmMass} < ${chipMass})`,
  icmMass < chipMass,
);

const multi = poker.nashMultiwayShoveCall({
  stackBb: 10,
  callerStacks: [10, 14],
  smallBlind: 0.5,
  bigBlind: 1,
  ante: 0,
});
assertTrue('multi jam 169', multi.jam.length === 169);
assertTrue('multi two callers', multi.calls.length === 2 && multi.calls[0].length === 169);
assertTrue('multi AA jams', multi.jam[aa] >= 0.9);

console.log(
  `OK: verify-nash-push-fold — AA@10bb=${jam10[aa].toFixed(2)} 72o@10bb=${jam10[o72].toFixed(2)} ` +
    `mass 8/10/12bb=${mass8}/${mass10}/${mass12} ICM ${icmMass}<${chipMass} chip`,
);
