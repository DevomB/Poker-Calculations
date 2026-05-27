import assert from 'node:assert/strict';
import test from 'node:test';
import { createRequire } from 'node:module';
import { packCards } from '../encode.js';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

const hole = ['Ah', 'Kh'];
const board = ['Qh', 'Jh', 'Th'];

test('evaluateHandStrength returns number', () => {
  const s = poker.evaluateHandStrength(hole, board);
  assert.equal(typeof s, 'number');
  assert.ok(s > 0);
});

test('evaluateHandStrengthFast returns number', () => {
  const s = poker.evaluateHandStrengthFast(hole, board);
  assert.equal(typeof s, 'number');
});

test('legacy and fast strength match on royal flush spot', () => {
  const legacy = poker.evaluateHandStrength(hole, board);
  const fast = poker.evaluateHandStrengthFast(hole, board);
  assert.equal(legacy, fast);
});

test('Scalar exports removed', () => {
  assert.equal(poker.evaluateHandStrengthScalar, undefined);
  assert.equal(poker.evaluateHandStrengthFastScalar, undefined);
});

test('packed strength matches strings', () => {
  const packedHole = packCards(hole);
  const packedBoard = packCards(board);
  const fromStrings = poker.evaluateHandStrength(hole, board);
  const fromPacked = poker.evaluateHandStrength(packedHole, packedBoard);
  assert.equal(fromPacked, fromStrings);
});

test('simulateHandOutcome packed vs strings', () => {
  const packedHole = packCards(hole);
  const packedBoard = packCards(board);
  const a = poker.simulateHandOutcome(hole, board, 500, 42, 1);
  const b = poker.simulateHandOutcome(packedHole, packedBoard, 500, 42, 1);
  assert.equal(a, b);
});

test('invalid packed byte throws', () => {
  const bad = new Uint8Array([52]);
  assert.throws(
    () => poker.evaluateHandStrength(bad, packCards(board)),
    /invalid (packed card at index 0|deck index at byte 0)/,
  );
});

test('compareBestHands packed vs string', () => {
  const a = ['Ah', 'Kh', 'Qh', 'Jh', 'Th'];
  const b = ['As', 'Ks', 'Qs', 'Js', 'Ts'];
  const cmpStr = poker.compareBestHands(a, b);
  const cmpPacked = poker.compareBestHands(packCards(a), packCards(b));
  assert.equal(cmpPacked, cmpStr);
});

test('exactHuEquityVsRandomHand packed vs string', () => {
  const flop = ['2c', '3d', '4h'];
  const eqStr = poker.exactHuEquityVsRandomHand(hole, flop);
  const eqPacked = poker.exactHuEquityVsRandomHand(packCards(hole), packCards(flop));
  assert.equal(eqPacked, eqStr);
});

test('cardStringsHaveDuplicate on packed bytes', () => {
  assert.equal(poker.cardStringsHaveDuplicate(packCards(['Ah', 'Kd'])), false);
  assert.equal(poker.cardStringsHaveDuplicate(packCards(['Ah', 'Ah'])), true);
});

test('decideAction accepts packed holeCards', () => {
  const state = {
    players: [
      {
        name: 'Hero',
        holeCards: packCards(['Ah', 'Kh']),
        stack: 200,
        committedThisStreet: 0,
        totalCommittedHand: 0,
        folded: false,
        seat: 0,
      },
      {
        name: 'Villain',
        holeCards: packCards(['2c', '3d']),
        stack: 200,
        committedThisStreet: 0,
        totalCommittedHand: 0,
        folded: false,
        seat: 1,
      },
    ],
    communityCards: packCards(['Qh', 'Jh', 'Th']),
    phase: 'flop',
    pot: 20,
    currentBet: 0,
    buttonSeat: 0,
    smallBlind: 1,
    bigBlind: 2,
    actingIndex: 0,
    actedThisStreet: [false, false],
  };
  const decision = poker.decideAction(state, { monteCarloSimulations: 0 });
  assert.ok(['fold', 'call', 'raise', 'check'].includes(decision.action));
});

const decideStateFixture = () => ({
  players: [
    {
      name: 'Hero',
      holeCards: packCards(['Ah', 'Kh']),
      stack: 200,
      committedThisStreet: 0,
      totalCommittedHand: 0,
      folded: false,
      seat: 0,
    },
    {
      name: 'Villain',
      holeCards: packCards(['2c', '3d']),
      stack: 200,
      committedThisStreet: 0,
      totalCommittedHand: 0,
      folded: false,
      seat: 1,
    },
  ],
  communityCards: packCards(['Qh', 'Jh', 'Th']),
  phase: 'flop',
  pot: 20,
  currentBet: 0,
  buttonSeat: 0,
  smallBlind: 1,
  bigBlind: 2,
  actingIndex: 0,
  actedThisStreet: [false, false],
});

test('simulateHandOutcomeAsync matches sync', async () => {
  const sync = poker.simulateHandOutcome(hole, board, 500, 42, 1);
  const async = await poker.simulateHandOutcomeAsync(hole, board, 500, 42, 1);
  assert.equal(async, sync);
});

test('parallelHandSimulationAsync matches sync', async () => {
  const sync = poker.parallelHandSimulation(hole, board, 500, 42, 1, 2);
  const async = await poker.parallelHandSimulationAsync(hole, board, 500, 42, 1, 2);
  assert.equal(async, sync);
});

test('exactHuEquityVsRandomHandAsync matches sync', async () => {
  const flop = ['2c', '3d', '4h'];
  const sync = poker.exactHuEquityVsRandomHand(hole, flop);
  const async = await poker.exactHuEquityVsRandomHandAsync(hole, flop);
  assert.equal(async, sync);
});

test('straightMadeFlopToRiverExactProbabilityAsync matches sync', async () => {
  const flop = ['2c', '3d', '4h'];
  const dead = [];
  const sync = poker.straightMadeFlopToRiverExactProbability(hole, flop, dead);
  const async = await poker.straightMadeFlopToRiverExactProbabilityAsync(hole, flop, dead);
  assert.equal(async, sync);
});

test('benchmarkEvaluatorThroughputAsync returns benchmark shape', async () => {
  const result = await poker.benchmarkEvaluatorThroughputAsync(1000);
  assert.equal(typeof result.legacyEvalsPerSecond, 'number');
  assert.equal(typeof result.fastEvalsPerSecond, 'number');
  assert.ok(result.fastEvalsPerSecond > 0);
  assert.equal(typeof result.implementation, 'string');
});

test('decideActionAsync matches sync', async () => {
  const state = decideStateFixture();
  const config = { monteCarloSimulations: 0 };
  const sync = poker.decideAction(state, config);
  const async = await poker.decideActionAsync(state, config);
  assert.equal(async.action, sync.action);
  assert.equal(async.raiseBy, sync.raiseBy);
});

test('simulateHandOutcomeAsync does not block the event loop', async () => {
  let ticked = false;
  setImmediate(() => {
    ticked = true;
  });
  const pending = poker.simulateHandOutcomeAsync(hole, board, 200_000, 42, 1);
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(ticked, true, 'setImmediate should run before MC work finishes');
  const eq = await pending;
  assert.equal(typeof eq, 'number');
});

test('simulateHandOutcomeAsync throws on invalid packed cards before queueing', () => {
  const bad = new Uint8Array([52]);
  assert.throws(
    () => poker.simulateHandOutcomeAsync(bad, packCards(board), 100, 42, 1),
    /invalid (packed card|deck index)/,
  );
});
