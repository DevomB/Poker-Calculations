/**
 * FEATURES_ADDED: Sizing & commitment — betAsPotFraction, sprAfterCall, commitmentRatio
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Sizing & commitment ---');

console.log('betAsPotFraction pot 100 bet 33:', poker.betAsPotFraction(100, 33));
console.log('sprAfterCall pot 100 call 50 eff stack 200:', poker.sprAfterCall(100, 50, 200));
console.log('commitmentRatio call 50 into 200 eff:', poker.commitmentRatio(50, 200));
