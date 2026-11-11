'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const poker = require('..');

function spot(overrides = {}) {
  return {
    players: [
      { holeCards: ['Ah', 'Kh'], stack: 200, seat: 0, committedThisStreet: 0 },
      { holeCards: ['2c', '3d'], stack: 200, seat: 1, committedThisStreet: 10 },
    ],
    communityCards: ['Qh', 'Jh', 'Th'],
    phase: 'flop', pot: 100, currentBet: 10, bigBlind: 2,
    lastRaiseIncrement: 30, actingIndex: 0, actedThisStreet: [false, true],
    ...overrides,
  };
}

const config = { monteCarloSimulations: 16, rngSeed: 42 };

test('every policy entry point respects minimum raises and the remaining stack', async () => {
  for (const [stack, committed, currentBet, pot, minimum, expected] of [
    [200, 0, 10, 10, 30, 30], // Full minimum raise even when pot sizing is smaller.
    [20, 0, 10, 100, 30, 10], // Short all-in raise above a call.
    [5, 0, 0, 100, 2, 5], // Opening bet capped at the stack.
    [1, 0, 0, 100, 2, 1], // Opening all-in below a big blind.
    [16, 4, 10, 100, 30, 10], // Existing commitment is not charged twice.
    [200, 0, 10, 100, 2, 55], // Ordinary pot-sized policy behavior is preserved.
  ]) {
    const state = spot({ currentBet, pot, lastRaiseIncrement: minimum });
    Object.assign(state.players[0], { stack, committedThisStreet: committed });
    const wanted = { action: 'raise', raiseBy: expected };
    for (const input of [state, poker.encodePokerState(state)]) {
      assert.deepEqual(poker.decideAction(input, config), wanted);
      assert.deepEqual(await poker.decideActionAsync(input, config), wanted);
      assert.deepEqual(poker.decideActionWithDiagnostics(input, config).decision, wanted);
      assert.deepEqual(poker.runBotPolicyBatch([input], config)[0].decision, wanted);
    }
  }
  const allInCall = spot();
  allInCall.players[0].stack = 10;
  assert.deepEqual(poker.decideAction(allInCall, config), { action: 'call', raiseBy: 0 });
});

test('raise sizing rejects non-finite fractions and caps huge finite sizes before conversion', () => {
  const state = spot();
  for (const raisePotFraction of [NaN, Infinity, -Infinity]) {
    assert.throws(() => poker.decideAction(state, { ...config, raisePotFraction }), /finite/);
  }
  assert.deepEqual(poker.decideAction(state, { ...config, raisePotFraction: 1e30 }),
    { action: 'raise', raiseBy: 190 });
});

test('batch opponent models agree with individual decisions, including null models', () => {
  const state = spot({ communityCards: ['2c', '5d', '8s', '9c', 'Jh'], currentBet: 0, pot: 20 });
  state.players[0].holeCards = ['Ah', 'Kd'];
  state.players[1].holeCards = [];
  const cfg = { ...config, monteCarloSimulations: 2000, aggressionThreshold: 0.5, opponentAggressionWeight: 1 };
  const models = [{ aggressionFactor: 0 }, { aggressionFactor: 1 }, null];
  const expected = models.map((model) => poker.decideAction(state, cfg, model));
  assert.notDeepEqual(expected[0], expected[1], 'fixture must exercise model-dependent behavior');
  assert.deepEqual(poker.runBotPolicyBatch(models.map(() => state), cfg, models).map((x) => x.decision), expected);
  assert.deepEqual(poker.runBotPolicyBatch([state], cfg)[0].decision, expected[2]);
  assert.throws(() => poker.runBotPolicyBatch([state], cfg, []), /opponentModels/);
  assert.throws(() => poker.runBotPolicyBatch([state], cfg, {}), /opponentModels/);
  assert.throws(() => poker.runBotPolicyBatch([state], cfg, [42]), /opponentModels/);
});

test('no actions are advertised for finished hands or unavailable actors', () => {
  const states = [spot({ phase: 'showdown' }), spot({ phase: 'handcomplete' }),
    spot({ actingIndex: -1 }), spot({ actingIndex: 99 }), spot({ players: [], actingIndex: -1 })];
  for (const change of [{ folded: true }, { stack: 0 }, { stack: -1 }]) {
    const state = spot();
    Object.assign(state.players[0], change);
    states.push(state);
  }
  for (const state of states) {
    const legal = poker.legalActionSummary(state);
    assert.deepEqual([legal.canFold, legal.canCheck, legal.canCall, legal.canRaise], [false, false, false, false]);
    assert.equal(poker.actionMaskFromState(state), 0);
  }
  const short = spot();
  short.players[0].stack = 20;
  assert.equal(poker.legalActionSummary(short).minRaiseTo, 20, 'short all-in is the only possible raise');
  const noCaller = spot({ currentBet: 0 });
  noCaller.players[1].stack = 0;
  assert.equal(poker.legalActionSummary(noCaller).canRaise, false);
});

test('diagnostic legality follows the explicitly requested hero seat', () => {
  const state = spot({ actingIndex: 1 });
  const actual = poker.decideActionWithDiagnostics(state, config, null, 0);
  assert.deepEqual(actual.legalActions, poker.legalActionSummary({ ...state, actingIndex: 0 }));
});

test('all policy entry points refuse finished hands and unavailable heroes', async () => {
  const states = [spot({ phase: 'showdown' }), spot({ phase: 'handcomplete' }), spot({ actingIndex: -1 })];
  for (const change of [{ folded: true }, { stack: 0 }]) {
    const state = spot();
    Object.assign(state.players[0], change);
    states.push(state);
  }
  for (const state of states) {
    assert.throws(() => poker.decideAction(state, config), /requires/);
    assert.throws(() => poker.decideActionWithDiagnostics(state, config), /requires/);
    assert.throws(() => poker.runBotPolicyBatch([state], config), /requires/);
    await assert.rejects(poker.decideActionAsync(state, config), /requires/);
  }
  assert.throws(() => poker.decideAction(spot(), config, null, 99), /requires/);
});
