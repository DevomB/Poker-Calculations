/**
 * FEATURES_ADDED: GTO-style (toy) — minimumDefenseFrequency, alphaFrequency,
 * bluffToValueRatio, valueToBluffRatio
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- GTO toy frequencies ---');

const pot = 100;
const bet = 50;
console.log('minimumDefenseFrequency:', poker.minimumDefenseFrequency(pot, bet));
console.log('alphaFrequency (1 - MDF):', poker.alphaFrequency(pot, bet));
console.log('bluffToValueRatio:', poker.bluffToValueRatio(pot, bet));
console.log('valueToBluffRatio:', poker.valueToBluffRatio(pot, bet));
