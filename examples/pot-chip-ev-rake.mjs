/**
 * FEATURES_ADDED: Pot / chip EV — potOddsRatio, expectedValueCall, breakevenCallEquity,
 * rakeFromPot, breakevenCallEquityWithRake
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Pot odds, chip EV, rake ---');

console.log('potOddsRatio 100 pot call 50:', poker.potOddsRatio(100, 50));
console.log('expectedValueCall 40% equity pot 100 call 50:', poker.expectedValueCall(0.4, 100, 50));
console.log('breakevenCallEquity same geometry:', poker.breakevenCallEquity(100, 50));

const potAfterCall = 100 + 50 + 50;
const rake = poker.rakeFromPot(potAfterCall, 0.05, 25);
console.log(`rakeFromPot final pot ${potAfterCall} @ 5% cap 25:`, rake);

console.log(
  'breakevenCallEquityWithRake (pot 100, call 50, 5% rake cap 25):',
  poker.breakevenCallEquityWithRake(100, 50, 0.05, 25)
);
