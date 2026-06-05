'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');

const root = path.join(__dirname, '..');

const poker = require(root);

assert.equal(
  typeof poker.evaluateHandStrengthFast(['As', 'Kd'], ['2c', '7d', '9h', 'Ts', 'Jc']),
  'number',
);
assert.equal(poker.evaluateHandCategory(['8h', '9d'], ['6c', '7s', 'Td']), 'straight');
assert.equal(poker.evaluateHandCategory(['Qh', 'Qd'], ['Qc', '2d', '2h']), 'fullHouse');
assert.equal(poker.handRankCategoryOrder('highCard'), 0);
assert.equal(poker.handRankCategoryOrder('royalFlush'), 9);

console.log('Native smoke test passed.');
