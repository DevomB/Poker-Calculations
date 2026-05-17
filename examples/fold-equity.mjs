/**
 * FEATURES_ADDED: Fold equity — pure/semi bluff FE, rake variants, two-street pure bluff (P8),
 * breakevenFoldEquityFirstStreetPureBluff
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Fold equity ---');

console.log('breakevenFoldEquityPureBluff pot 100 jam 50:', poker.breakevenFoldEquityPureBluff(100, 50));
console.log(
  'breakevenFoldEquitySemiBluff:',
  poker.breakevenFoldEquitySemiBluff(100, 50, 0.25, 200)
);

const rakeF = 0.05;
const rakeCap = 25;
console.log(
  'breakevenFoldEquityPureBluffWithRake:',
  poker.breakevenFoldEquityPureBluffWithRake(100, 50, rakeF, rakeCap)
);
console.log(
  'breakevenFoldEquitySemiBluffWithRake:',
  poker.breakevenFoldEquitySemiBluffWithRake(100, 50, 0.2, 250, rakeF, rakeCap)
);

const p1 = 100;
const b1 = 50;
const b2 = 50;
const sameFe = poker.twoStreetPureBluffSameFoldEquity(p1, b1, b2);
console.log('twoStreetPureBluffSameFoldEquity:', sameFe);
if (Number.isFinite(sameFe) && sameFe < 1) {
  console.log('twoStreetPureBluffEv at same FE:', poker.twoStreetPureBluffEv(p1, b1, b2, sameFe, sameFe));
  console.log(
    'breakevenFoldEquitySecondStreetPureBluff (fe1 = sameFe):',
    poker.breakevenFoldEquitySecondStreetPureBluff(p1, b1, b2, sameFe)
  );
} else {
  console.log('(skip second-street / EV helpers: sameFe not finite or fe1 would be degenerate at 1)');
}

const fe2 = 0.35;
const fe1 = poker.breakevenFoldEquityFirstStreetPureBluff(p1, b1, b2, fe2);
console.log('breakevenFoldEquityFirstStreetPureBluff fe2=0.35 → fe1:', fe1);
console.log('twoStreetPureBluffEv at (fe1, fe2):', poker.twoStreetPureBluffEv(p1, b1, b2, fe1, fe2));
