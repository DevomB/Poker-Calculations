'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');
const { Worker, isMainThread } = require('node:worker_threads');

// Default: this checkout's entry point. Pass a directory to smoke an installed copy instead, e.g.
//   node scripts/smoke-load.js some-empty-dir/node_modules/poker-calculations
const target = process.argv[2] ? path.resolve(process.argv[2]) : path.join(__dirname, '..');

function check(poker) {
  const count = Object.keys(poker).filter((name) => typeof poker[name] === 'function').length;
  assert.equal(count, 300, 'native export count');
  const royal = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th', '2c', '3d']);
  assert.equal(royal.rank, 'royalFlush');
  assert.equal(poker.evaluateHandCategory(['9h', '9d'], ['9c', '5s', '5h']), 'fullHouse');

  const equity = poker.parallelHandSimulation(['Ah', 'Kd'], ['Qh', 'Jh', '2c'], 200, 7, 1, 2);
  assert.ok(Number.isFinite(equity) && equity >= 0 && equity <= 1, 'parallel equity in [0, 1]');
  const batch = poker.simulateHandOutcomeBatch([{
    holeCards: ['Ah', 'Kh'], board: ['Qh', 'Jh', 'Th'], numSimulations: 20, seed: 7,
  }]);
  assert.deepEqual(Array.from(batch), [1], 'royal flush wins every batch simulation');
  return `${count} functions; ${royal.rank}`;
}

async function main() {
  // Use the same entry point and node-gyp-build resolution as an installed package.
  const poker = require(target);
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
