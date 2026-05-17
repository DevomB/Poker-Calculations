/**
 * FEATURES_ADDED: Hand resolution — evaluateBestHand, evaluateHandStrength, evaluateHandCategory
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Hand resolution ---');

const best = poker.evaluateBestHand(['Ah', 'Kd', 'Qc', 'Js', 'Th', '2d', '3c']);
console.log('Best five of seven (broadway-ish):', best);

console.log('Best hand from two hole cards only:', poker.evaluateBestHand(['Ah', 'Kd']));

console.log('Strength AhKh on monotone flop:', poker.evaluateHandStrength(['Ah', 'Kh'], ['Qh', 'Jh', '2h']));
console.log('Category same spot:', poker.evaluateHandCategory(['Ah', 'Kh'], ['Qh', 'Jh', '2h']));

console.log('Preflop strength (empty board):', poker.evaluateHandStrength(['Ah', 'Kd'], []));
console.log('Preflop category (empty board):', poker.evaluateHandCategory(['Ah', 'Kd'], []));
