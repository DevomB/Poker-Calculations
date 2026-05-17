/**
 * FEATURES_ADDED: Monte Carlo equity — simulateHandOutcome, parallelHandSimulation, exactHuEquityVsRandomHand
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Monte Carlo equity ---');

const mc = poker.simulateHandOutcome(['Ah', 'Kh'], [], 2_000, 12_345, 1);
console.log('Preflop AKs vs 1 random villain (MC ~2000):', mc);

const mc2 = poker.simulateHandOutcome(['Ah', 'Kh'], [], 1_500, 12_346, 2);
console.log('Preflop AKs vs 2 random villains (MC ~1500):', mc2);

const par = poker.parallelHandSimulation(['Ah', 'Kh'], ['Qd', 'Jc', '2h'], 4_000, 99_001, 1, 2);
console.log('Flop AK vs 1 villain, 2 threads:', par);

const par2 = poker.parallelHandSimulation(['Ah', 'Kh'], ['Qd', 'Jc', '2h'], 3_000, 99_002, 2, 2);
console.log('Flop AK vs 2 villains, 2 threads:', par2);

const exactFlop = poker.exactHuEquityVsRandomHand(['As', 'Ks'], ['Qs', 'Js', '2c']);
console.log('Exact HU vs random (flop only, 3 board cards):', exactFlop);

const exactTurn = poker.exactHuEquityVsRandomHand(['As', 'Ah'], ['Kd', 'Qc', '7h', '5s']);
console.log('Exact HU vs random (turn, 4 board cards):', exactTurn);

const exact = poker.exactHuEquityVsRandomHand(['As', 'Ah'], ['Kd', 'Qc', '7h', '5s', '3d']);
console.log('Exact HU vs random (river board):', exact);
