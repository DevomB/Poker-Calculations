/**
 * FEATURES_ADDED: Reverse implied / geometry — reverseImpliedOddsMaxFutureLoss,
 * geometricPotAfterMatchedPotFractions
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Reverse implied & geometric pot ---');

console.log(
  'reverseImpliedOddsMaxFutureLoss pot100 call50 equity 0.3:',
  poker.reverseImpliedOddsMaxFutureLoss(100, 50, 0.3)
);

const pot0 = 100;
const f = 0.5;
const n = 3;
console.log(
  `geometricPotAfterMatchedPotFractions pot0=${pot0}, f=${f}, n=${n}:`,
  poker.geometricPotAfterMatchedPotFractions(pot0, f, n)
);
