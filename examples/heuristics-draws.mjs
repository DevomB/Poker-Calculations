/**
 * FEATURES_ADDED: Heuristics — rule of 4/2, implied breakeven future win, hypergeometric,
 * runner-runner flush, flop→river unions (2/3/4 categories, disjoint sum), runner straight pattern,
 * straightMadeFlopToRiverExactProbability, duplicationAdjustedOuts
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Heuristics & draw math ---');

console.log('ruleOfFourEquity(9):', poker.ruleOfFourEquity(9));
console.log('ruleOfTwoEquity(9):', poker.ruleOfTwoEquity(9));

console.log(
  'impliedBreakevenFutureWin pot100 call50 equity 0.35:',
  poker.impliedBreakevenFutureWin(100, 50, 0.35)
);

console.log('hypergeometric 9/47:', poker.hypergeometricOneCardHitProbability(9, 47));
console.log('runner-runner flush 10 suited of 47 unseen:', poker.runnerRunnerBackdoorFlushTwoCardProbability(10, 47));

const unseen = 47;
console.log('flopToRiverAtLeastOneHitProbability(9, 47):', poker.flopToRiverAtLeastOneHitProbability(9, unseen));
console.log(
  'flopToRiverAtLeastOneHitUnionTwoCategories (9 flush + 4 straight, shared 1):',
  poker.flopToRiverAtLeastOneHitUnionTwoCategories(unseen, 9, 4, 1)
);
console.log(
  'flopToRiverAtLeastOneHitUnionThreeCategories (small toy overlaps):',
  poker.flopToRiverAtLeastOneHitUnionThreeCategories(unseen, 4, 4, 4, 1, 1, 1, 0)
);

// Disjoint four categories of 2 outs (matches C++ test PokerMath.UnionFourCategoriesMatchesDisjointSum)
const pUnion4 = poker.flopToRiverAtLeastOneHitUnionFourCategories(
  unseen,
  2,
  2,
  2,
  2,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
);
const pDisjoint = poker.flopToRiverAtLeastOneHitDisjointOutsSum(unseen, [2, 2, 2, 2]);
console.log('Union four vs disjoint sum (should match):', pUnion4, pDisjoint);

console.log(
  'runnerRunnerStraightDrawHitProbability gutshot (0), 0 dead, 47 unseen:',
  poker.runnerRunnerStraightDrawHitProbability(0, 0, 47)
);
console.log(
  'runnerRunnerStraightDrawHitProbability OESD kind (1), 0 dead, 47 unseen:',
  poker.runnerRunnerStraightDrawHitProbability(1, 0, 47)
);
console.log(
  'runnerRunnerStraightDrawHitProbability double-belly kind (2), 0 dead, 47 unseen:',
  poker.runnerRunnerStraightDrawHitProbability(2, 0, 47)
);

const straightExact = poker.straightMadeFlopToRiverExactProbability(
  ['Ah', 'Kd'],
  ['Qc', 'Js', '2h'],
  []
);
console.log('straightMadeFlopToRiverExactProbability (OESD flop sample):', straightExact);

console.log(
  'straightMadeFlopToRiverExactProbability with knownDead (excluded cards):',
  poker.straightMadeFlopToRiverExactProbability(['Ah', 'Kd'], ['Qc', 'Js', '2h'], ['Ac', 'Ad'])
);

console.log('duplicationAdjustedOuts(9 outs, 2 villains, weight 0.25):', poker.duplicationAdjustedOuts(9, 2, 0.25));
