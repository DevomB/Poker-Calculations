'use strict';

// Runs each scripts/verify-*.mjs as one test. They run in this process, so they share one load of
// the addon and its caches (the Nash equity table, for example); a script fails by throwing.

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const dir = path.join(__dirname, '..', 'scripts');
const scripts = fs
  .readdirSync(dir)
  .filter((f) => /^verify-.+\.mjs$/.test(f) && f !== 'verify-lib.mjs')
  .sort();

test('verify scripts are found', () => {
  // Guards against a pattern that silently matches nothing.
  assert.ok(scripts.length >= 17, `found ${scripts.length}: ${scripts.join(', ')}`);
});

for (const file of scripts) {
  test(file, async () => {
    await import(pathToFileURL(path.join(dir, file)).href);
  });
}

test('verify assertions reject NaN', async () => {
  const { assertNear } = await import(pathToFileURL(path.join(dir, 'verify-lib.mjs')).href);
  assert.throws(() => assertNear('nan', Number.NaN, 0.5, 1));
  assert.throws(() => assertNear('infinite vs finite', Infinity, 0.5, 1));
  assertNear('matching infinities', Infinity, Infinity);
  assertNear('within tolerance', 0.5 + 1e-12, 0.5);
});
