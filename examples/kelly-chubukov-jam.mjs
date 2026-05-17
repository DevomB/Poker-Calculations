/**
 * FEATURES_ADDED: Kelly & Chubukov jam toys — kellyCriterionBinary, chubukovSymmetricJamBreakevenStack,
 * chubukovSymmetricJamEv, chubukovMaxSymmetricJamStackChipsBinarySearch,
 * chubukovMaxSymmetricJamStackBinarySearch, chubukovMaxSymmetricJamStackFromHandBinarySearch
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Kelly & Chubukov ---');

// Win 1:1 net (b=1), p=0.55 → positive edge
console.log('kellyCriterionBinary p=0.55, netOdds=1:', poker.kellyCriterionBinary(0.55, 1));

const dead = 150;
const eq = 0.42;
console.log('chubukovSymmetricJamBreakevenStack dead=150 equity=0.42:', poker.chubukovSymmetricJamBreakevenStack(dead, eq));
console.log('chubukovSymmetricJamEv jam=80:', poker.chubukovSymmetricJamEv(80, dead, eq));
console.log(
  'chubukovMaxSymmetricJamStackChipsBinarySearch equity=0.4 dead=100 max=500:',
  poker.chubukovMaxSymmetricJamStackChipsBinarySearch(0.4, 100, 500)
);

const hero = ['As', 'Ks'];
const board = ['Qs', 'Js', '2c'];
const maxCap = 1_000_000_000;

const maxJamFloat = poker.chubukovMaxSymmetricJamStackBinarySearch(hero, board, dead, maxCap);
const maxJamIntPath = poker.chubukovMaxSymmetricJamStackFromHandBinarySearch(hero, board, dead, maxCap);
console.log('chubukovMaxSymmetricJamStackBinarySearch (AKs monotone flop):', maxJamFloat);
console.log(
  'chubukovMaxSymmetricJamStackFromHandBinarySearch (same hand; int32 path in native):',
  maxJamIntPath
);
