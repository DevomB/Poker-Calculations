/**
 * Suit isomorphism / board canonicalization checks.
 * Run from NPM/: `node scripts/verify-suit-isomorphism.mjs` (requires `npm run build:native`).
 *
 * Orbit sizes (unordered 3-card sets under S4):
 * - unpaired rainbow (3 ranks, 3 suits): 24
 * - unpaired two-tone: 12
 * - unpaired monotone: 4
 * - pair + kicker (3 suits or 2): 12; trips: 4
 * Rank ties shrink the orbit; sizes always divide 24.
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

function sameCards(a, b) {
  return JSON.stringify(a) === JSON.stringify(b);
}

const names = [
  'canonicalFlopBoard',
  'canonicalBoard',
  'canonicalHolesAndBoard',
  'suitPermFromCanonicalFlop',
  'applySuitPermToCards',
  'applySuitPermToRange1326',
  'isomorphicFlopOrbitSize',
  'countCanonicalFlops',
  'isomorphicFlopIndex',
  'flopIndexToCanonical',
];
for (const name of names) {
  assertTrue(`${name} exported`, typeof poker[name] === 'function');
}

assertTrue('countCanonicalFlops === 1755', poker.countCanonicalFlops() === 1755);

const hearts = poker.canonicalFlopBoard(['Ah', 'Kh', '9h']);
const diamonds = poker.canonicalFlopBoard(['Ad', 'Kd', '9d']);
assertTrue(
  `AhKh9h and AdKd9d canonicalize equal (${hearts} vs ${diamonds})`,
  sameCards(hearts, diamonds),
);

const rainbow = ['Ah', 'Kd', '9c'];
assertTrue('rainbow unpaired orbit 24', poker.isomorphicFlopOrbitSize(rainbow) === 24);
assertTrue('two-tone unpaired orbit 12', poker.isomorphicFlopOrbitSize(['Ah', 'Kh', '9c']) === 12);
assertTrue('monotone unpaired orbit 4', poker.isomorphicFlopOrbitSize(['Ah', 'Kh', '9h']) === 4);
assertTrue('paired 3-suit orbit 12', poker.isomorphicFlopOrbitSize(['Ah', 'Ad', 'Kc']) === 12);
assertTrue('trips orbit 4', poker.isomorphicFlopOrbitSize(['Ah', 'Ad', 'Ac']) === 4);

const perm = poker.suitPermFromCanonicalFlop(['Ah', 'Kh', '9h']);
assertTrue('suit perm length 4', perm.length === 4);
const remapped = poker.applySuitPermToCards(['Ah', 'Kh', '9h'], perm);
assertTrue('apply perm matches canonical flop', sameCards(remapped.slice().sort(), hearts.slice().sort()));

const flopOnly = poker.canonicalFlopBoard(['Qh', 'Jd', '2c']);
const board3 = poker.canonicalBoard(['Qh', 'Jd', '2c']);
assertTrue('canonicalBoard 3-card matches flop', sameCards(flopOnly, board3));

const turnBoard = poker.canonicalBoard(['Ah', 'Kh', '9c', 'Qd']);
assertTrue('canonicalBoard turn length 4', turnBoard.length === 4);

const joint = poker.canonicalHolesAndBoard(['As', 'Kd'], ['Ah', 'Kh', '9h']);
assertTrue('joint holes length 2', joint.holes.length === 2);
assertTrue('joint board length 3', joint.board.length === 3);
assertTrue('joint perm length 4', joint.suitPerm.length === 4);

const range = new Float64Array(1326);
for (let i = 0; i < 1326; i += 1) {
  range[i] = (i % 17) + 0.25;
}
let massIn = 0;
for (let i = 0; i < 1326; i += 1) massIn += range[i];
const mapped = poker.applySuitPermToRange1326(range, perm);
let massOut = 0;
for (let i = 0; i < 1326; i += 1) massOut += mapped[i];
assertTrue(`range mass preserved (${massIn} vs ${massOut})`, Math.abs(massIn - massOut) < 1e-9);
assertTrue('mapped range length 1326', mapped.length === 1326);

const identity = poker.applySuitPermToRange1326(range, [0, 1, 2, 3]);
for (let i = 0; i < 1326; i += 1) {
  assertTrue(`identity range[${i}]`, identity[i] === range[i]);
}

for (let i = 0; i < 1755; i += 1) {
  const canon = poker.flopIndexToCanonical(i);
  assertTrue(`index ${i} length 3`, canon.length === 3);
  const back = poker.isomorphicFlopIndex(canon);
  assertTrue(`index round-trip ${i} -> ${back}`, back === i);
  const again = poker.canonicalFlopBoard(canon);
  assertTrue(`canonical flop at ${i} is already canonical`, sameCards(again, canon));
}

const idxHearts = poker.isomorphicFlopIndex(['Ah', 'Kh', '9h']);
const idxDiamonds = poker.isomorphicFlopIndex(['Ad', 'Kd', '9d']);
assertTrue('suited monotone flops share index', idxHearts === idxDiamonds);

console.log(
  `OK: verify-suit-isomorphism — 1755 flops, AhKh9h=AdKd9d=${hearts.join('')}, ` +
    `rainbow orbit=${poker.isomorphicFlopOrbitSize(rainbow)}`,
);
