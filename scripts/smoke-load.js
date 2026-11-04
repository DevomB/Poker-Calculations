'use strict';

const assert = require('node:assert/strict');
const { createRequire } = require('node:module');
const path = require('node:path');
const { Worker, isMainThread } = require('node:worker_threads');

// Default: this checkout's entry point. Pass a directory to smoke an installed copy instead, e.g.
//   node scripts/smoke-load.js some-empty-dir/node_modules/poker-calculations
const target = process.argv[2] ? path.resolve(process.argv[2]) : path.join(__dirname, '..');
// Resolve through the package's own "exports" map, as a consumer's require() does.
const requireFromPackage = createRequire(path.join(target, 'package.json'));

// Behavior fixed in recent releases, checked on the binary and files that actually ship.
function checkReleaseFixes(poker, encode) {
  const id = (c) => encode.cardStringToDeckIndex(c);
  const ci = (a, b) => { const lo = Math.min(a, b); const hi = Math.max(a, b); return lo * 51 - (lo * (lo - 1)) / 2 + (hi - lo - 1); };
  // Range weights are relative: [1, 1] and [1e308, 1e308] are the same range.
  for (const s of [1, 1e308]) {
    const range = new Float64Array(1326);
    range[ci(id('Qh'), id('Qd'))] = s;
    range[ci(id('5c'), id('4c'))] = s;
    assert.equal(poker.exactHuEquityVsRange(['Ah', 'Kd'], ['Qs', '7c', '2d', '9h', '3s'], range), 0.5, `range scale ${s}`);
  }
  assert.throws(() => poker.exactHuEquityVsRange(['Ah', 'Kd'], ['Qs', '7c', '2d', '9h', '3s'], new Float64Array(1326).fill(NaN)), /finite and non-negative/);
  // Wilson interval: 100 of 100 reaches 1.
  const w = poker.wilsonScoreInterval(100, 100, 1.96);
  assert.equal(w.upper, 1);
  assert.ok(Math.abs(w.lower - 0.96301) < 5e-6, `Wilson lower ${w.lower}`);
  // No raise against a player who is all in.
  const state = {
    players: [
      { holeCards: ['As', 'Ks'], stack: 200, seat: 0 },
      { holeCards: ['7c', '7d'], stack: 0, seat: 1, committedThisStreet: 10 },
    ],
    communityCards: ['Qs', 'Js', 'Ts'], phase: 'flop', pot: 30, currentBet: 10, actingIndex: 0, actedThisStreet: [false, true],
  };
  assert.equal(poker.legalActionSummary(state).canRaise, false);
  assert.equal(poker.decideAction(state, { monteCarloSimulations: 200, rngSeed: 1 }, null, 0).action, 'call');
  // One strict card rule in native code and in poker-calculations/encode.
  assert.equal(poker.canonicalCardString('ah'), 'Ah');
  assert.deepEqual(Array.from(encode.packCards(['ah', '10h'])), [50, 34]);
  assert.throws(() => encode.cardStringToDeckIndex('Ahjunk'));
  assert.throws(() => poker.canonicalCardString('Ahjunk'));
  assert.throws(() => poker.evaluateBestHand(new Float64Array([50, 46])), /Uint8Array/);
  assert.deepEqual(encode.unpackPokerState(encode.packPokerState(state)).players[1].holeCards, ['7c', '7d']);
  // Impossible inputs are rejected instead of answered.
  assert.throws(() => poker.flushMadeFlopToRiverExactProbability(['Ah', 'Kh'], ['Qh', '7h', '2c'], ['Ah']), /duplicate/);
  assert.throws(() => poker.benchmarkEvaluatorThroughput(Number.NaN), /iterations/);
}

function check(poker) {
  const count = Object.keys(poker).filter((name) => typeof poker[name] === 'function').length;
  assert.equal(count, 371, 'native export count');
  const royal = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th', '2c', '3d']);
  assert.equal(royal.rank, 'royalFlush');
  assert.equal(poker.evaluateHandCategory(['9h', '9d'], ['9c', '5s', '5h']), 'fullHouse');

  const equity = poker.parallelHandSimulation(['Ah', 'Kd'], ['Qh', 'Jh', '2c'], 200, 7, 1, 2);
  assert.ok(Number.isFinite(equity) && equity >= 0 && equity <= 1, 'parallel equity in [0, 1]');
  const batch = poker.simulateHandOutcomeBatch([{
    holeCards: ['Ah', 'Kh'], board: ['Qh', 'Jh', 'Th'], numSimulations: 20, seed: 7,
  }]);
  assert.deepEqual(Array.from(batch), [1], 'royal flush wins every batch simulation');
  checkReleaseFixes(poker, requireFromPackage('poker-calculations/encode'));
  return `${count} functions; ${royal.rank}; release checks and encode passed`;
}

async function main() {
  // Use the same entry point and node-gyp-build resolution as an installed package.
  const poker = requireFromPackage('poker-calculations');
  assert.equal(poker, require(target), 'exports map and package directory load the same module');
  check(poker);
  if (isMainThread) {
    await Promise.all([0, 1].map(() => new Promise((resolve, reject) => {
      const worker = new Worker(__filename, { argv: process.argv.slice(2) });
      worker.once('error', reject);
      worker.once('exit', (code) => {
        if (code === 0) resolve();
        else reject(new Error(`Smoke worker exited with code ${code}`));
      });
    })));
    // Worker initialization and teardown must not invalidate the main environment's cache.
    console.log(`OK: smoke-load - ${check(poker)}; batch and workers passed (${target})`);
  }
}

main().catch((err) => {
  console.error(err.stack || err);
  process.exitCode = 1;
});
