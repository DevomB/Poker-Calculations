/**
 * Runnable smoke test for `poker-calculations` (CommonJS entry via createRequire).
 * API surface: ../index.d.ts
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

const aa = poker.evaluateBestHand(['Ah', 'Ad']);
console.log('AA best-of-two:', aa);

const eq = poker.simulateHandOutcome(['Ah', 'Kh'], [], 500, 12345, 1);
console.log('Preflop AK equity vs 1 random (approx):', eq);

const state = {
  players: [
    {
      name: 'Hero',
      holeCards: ['Ah', 'Kd'],
      stack: 200,
      committedThisStreet: 0,
      totalCommittedHand: 0,
      folded: false,
      seat: 0,
    },
    {
      name: 'Villain',
      holeCards: ['2c', '7d'],
      stack: 200,
      committedThisStreet: 0,
      totalCommittedHand: 0,
      folded: false,
      seat: 1,
    },
  ],
  communityCards: [],
  phase: 'PreFlop',
  pot: 3,
  currentBet: 2,
  buttonSeat: 0,
  smallBlind: 1,
  bigBlind: 2,
  actingIndex: 0,
  lastRaiseIncrement: 0,
  streetOpeningIndex: 0,
  actedThisStreet: [false, false],
};

const cfg = {
  monteCarloSimulations: 300,
  rngSeed: 42,
};

const d = poker.decideAction(state, cfg, null, 0);
console.log('Decision:', d);

console.log('SPR pot 90 eff 270:', poker.spr(90, 270));
console.log('Breakeven equity calling 50 into 100+50:', poker.breakevenCallEquity(100, 50));
console.log('MDF facing bet 50 into pot 100:', poker.minimumDefenseFrequency(100, 50));
console.log('Alpha (1 - MDF) same spot:', poker.alphaFrequency(100, 50));
console.log('Rule of 4 (9 outs):', poker.ruleOfFourEquity(9));
console.log('EV call 40% equity pot 100 call 50:', poker.expectedValueCall(0.4, 100, 50));
console.log('SPR after call pot 100 call 50 eff stack 200:', poker.sprAfterCall(100, 50, 200));
console.log('Breakeven FE pure bluff 100 pot jam 50:', poker.breakevenFoldEquityPureBluff(100, 50));
console.log('Hypergeometric one card 9/47:', poker.hypergeometricOneCardHitProbability(9, 47));
console.log('Harrington M:', poker.harringtonM(1500, 50, 100, 0));
console.log('ICM win probs [400,300,300]:', poker.icmWinProbabilitiesHarville([400, 300, 300]));
console.log(
  'Exact river AA vs random (sample board):',
  poker.exactHuEquityVsRandomHand(['As', 'Ah'], ['Kd', 'Qc', '7h', '5s', '3d'])
);
