/**
 * Mixed hand-class ranges must not abort simulateEquityVsRange (exit 127).
 * Run from NPM/: `node scripts/verify-simulate-equity-vs-range.mjs` (requires built native addon).
 */
import { poker, assertTrue } from './verify-lib.mjs';

function assertFiniteUnit(label, value) {
  assertTrue(`${label} is finite`, Number.isFinite(value));
  assertTrue(`${label} in [0, 1]`, value >= 0 && value <= 1);
}

const hero = ['Ah', 'Kd'];
const flop = ['Qs', '7c', '2d'];
const turn = [...flop, 'Tc'];
const river = [...turn, '3s'];

const singleClass = poker.rangeFromNotationWeights([{ notation: 'AA', weight: 1 }]);
const mixedClass = poker.rangeFromNotationWeights([
  { notation: 'AA', weight: 1 },
  { notation: 'AKs', weight: 1 },
  { notation: '72o', weight: 1 },
]);
const mixedSparse = poker.pruneRangeByMinWeight(mixedClass, 1e-12);

assertTrue('single-class dense length', singleClass.length === 1326);
assertTrue('mixed-class dense length', mixedClass.length === 1326);
assertTrue('mixed sparse has combos', mixedSparse.indices.length >= 2 && mixedSparse.weights.length >= 1);

const cases = [
  ['single-class flop dense', flop, singleClass],
  ['mixed-class preflop dense', [], mixedClass],
  ['mixed-class flop dense', flop, mixedClass],
  ['mixed-class turn dense', turn, mixedClass],
  ['mixed-class river dense', river, mixedClass],
  ['mixed-class flop sparse', flop, mixedSparse],
  ['mixed-class turn sparse', turn, mixedSparse],
];

for (const [label, board, range] of cases) {
  const eq = poker.simulateEquityVsRange(hero, board, range, 20, 7);
  assertFiniteUnit(label, eq);
}

console.log('OK: verify-simulate-equity-vs-range');
