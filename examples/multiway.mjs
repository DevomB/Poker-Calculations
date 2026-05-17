/**
 * FEATURES_ADDED: Multiway — multiwaySymmetricBreakevenCallEquity,
 * multiwaySymmetricBreakevenCallEquityWithShare
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Multiway ---');

const pot = 100;
const toCall = 50;
const k = 1;
const wta = poker.multiwaySymmetricBreakevenCallEquity(pot, toCall, k);
console.log('multiwaySymmetricBreakevenCallEquity k=1:', wta);

const fixedShare = poker.multiwaySymmetricBreakevenCallEquityWithShare(pot, toCall, k, 1, 0.5);
console.log('WithShare model=1 heroFractionWhenWin=0.5:', fixedShare);

const winnerTakesAll = poker.multiwaySymmetricBreakevenCallEquityWithShare(pot, toCall, k, 0, 1);
console.log('WithShare model=0 (WTA):', winnerTakesAll);

const k2 = 2;
console.log('multiwaySymmetricBreakevenCallEquity k=2 extra callers:', poker.multiwaySymmetricBreakevenCallEquity(pot, toCall, k2));
console.log(
  'multiwaySymmetricBreakevenCallEquityWithShare k=2 WTA:',
  poker.multiwaySymmetricBreakevenCallEquityWithShare(pot, toCall, k2, 0, 1)
);
