/**
 * FEATURES_ADDED: Strategy — decideAction (MC equity / strength + BotConfig + optional OpponentModel)
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Strategy: decideAction ---');

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
  aggressionThreshold: 0.55,
  riskTolerance: 0.35,
  monteCarloSimulations: 400,
  monteCarloVillains: 1,
  raisePotFraction: 0.65,
  opponentAggressionWeight: 0.2,
  rngSeed: 42,
};

const decision = poker.decideAction(state, cfg, null, 0);
console.log('Hero decision (full BotConfig surface):', decision);

const withModel = poker.decideAction(state, cfg, { foldFrequency: 0.6, callFrequency: 0.25, aggressionFactor: 1.2 }, 0);
console.log('With opponent model:', withModel);

const postflop = {
  ...state,
  communityCards: ['Qd', 'Jc', '2h'],
  phase: 'Flop',
  pot: 8,
  currentBet: 0,
  actingIndex: 0,
  actedThisStreet: [false, false],
};
console.log('Postflop spot (same hero seat 0):', poker.decideAction(postflop, cfg, null, 0));
