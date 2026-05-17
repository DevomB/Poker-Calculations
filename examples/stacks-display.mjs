/**
 * FEATURES_ADDED: Stacks & display — spr, effectiveStack, stackInBigBlinds, potOddsRatioDisplay,
 * formatPotOdds, harringtonM, harringtonMEffective, harringtonMEffectiveActiveAntes
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Stacks & display ---');

console.log('SPR pot 90 eff 270:', poker.spr(90, 270));
console.log('effectiveStack 400, 250, 600:', poker.effectiveStack(400, 250, 600));
console.log('effectiveStack (no args):', poker.effectiveStack());
console.log('stackInBigBlinds 2000 @ BB 25:', poker.stackInBigBlinds(2000, 25));

console.log('potOddsRatioDisplay 100, 50:', poker.potOddsRatioDisplay(100, 50));
console.log('formatPotOdds 100, 50:', poker.formatPotOdds(100, 50, 2));

console.log('harringtonM stack 1500 sb50 bb100 antes 0:', poker.harringtonM(1500, 50, 100, 0));
console.log('harringtonMEffective (2 players × 25 ante):', poker.harringtonMEffective(1500, 50, 100, 25, 2));
console.log(
  'harringtonMEffectiveActiveAntes [1,2,1] active:',
  poker.harringtonMEffectiveActiveAntes(400, 1, 2, [1, 2, 1])
);
