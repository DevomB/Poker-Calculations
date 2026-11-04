'use strict';

// Helpers that look at a flop or turn before the board is complete. References are brute force with
// the general evaluator (`evaluateBestHand` / `evaluateHandStrength`), which scores exactly the cards
// it is given.

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');

const poker = require(path.join(__dirname, '..'));

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';
const DECK = [];
for (const r of RANKS) for (const s of SUITS) DECK.push(r + s);
const id = (c) => RANKS.indexOf(c[0]) * 4 + SUITS.indexOf(c[1]);
const CATS = ['highCard', 'onePair', 'twoPair', 'threeOfAKind', 'straight', 'flush', 'fullHouse', 'fourOfAKind', 'straightFlush', 'royalFlush'];
const category = (cards) => Math.min(CATS.indexOf(poker.evaluateBestHand(cards).rank), 8);
const strength = (hole, board) => poker.evaluateHandStrength(hole, board);

const HERO = ['Ah', 'Kh'];
const FLOP = ['2h', '7h', '9c'];
const LIVE = DECK.filter((c) => ![...HERO, ...FLOP].includes(c));

function near(actual, expected, tol, label) {
  assert.ok(Math.abs(actual - expected) <= tol, `${label}: expected ${expected}, got ${actual}`);
}

test('the turn category counts six real cards, not phantom 2c fillers', () => {
  // Every ordered (turn, river) deal is equally likely.
  const expected = new Array(81).fill(0);
  let deals = 0;
  for (const t of LIVE) {
    const ct = category([...HERO, ...FLOP, t]);
    for (const r of LIVE) {
      if (r === t) continue;
      expected[ct * 9 + category([...HERO, ...FLOP, t, r])] += 1;
      deals++;
    }
  }
  const got = Array.from(poker.exactHeroCategoryJointFlopToRiver(HERO, FLOP).jointMatrix);
  got.forEach((v, i) => near(v, expected[i] / deals, 1e-12, `cell ${Math.floor(i / 9)},${i % 9}`));
  const turn = (row) => got.slice(row * 9, row * 9 + 9).reduce((a, b) => a + b, 0);
  near(turn(0), 24 / 47, 1e-12, 'turn high card'); // 51.0638%
  assert.equal(turn(2), 0, 'no two pair is possible on the turn');
  assert.equal(turn(3), 0, 'no trips are possible on the turn');
});

test('both dealing orders of a turn-river pair count', () => {
  // Only 3h and As are live: either can be the turn, so a turn flush (3h) and a turn pair (As) are
  // each 1/2, and the river is the same seven cards either way.
  const dead = LIVE.filter((c) => c !== '3h' && c !== 'As');
  const j = Array.from(poker.exactHeroCategoryJointFlopToRiver(HERO, FLOP, dead).jointMatrix);
  const flush = CATS.indexOf('flush');
  const pair = CATS.indexOf('onePair');
  assert.equal(j[flush * 9 + flush], 0.5);
  assert.equal(j[pair * 9 + flush], 0.5);
  assert.equal(j.reduce((a, b) => a + b, 0), 1);
});

test('leapfrog outs compare real five- and six-card hands', () => {
  const villainBest = (board, dead) => {
    const deck = DECK.filter((c) => !dead.includes(c));
    let best = 0;
    for (let i = 0; i < deck.length; i++) {
      for (let k = i + 1; k < deck.length; k++) best = Math.max(best, strength([deck[i], deck[k]], board));
    }
    return best;
  };
  const heroNow = strength(HERO, FLOP);
  const villainNow = villainBest(FLOP, [...HERO, ...FLOP]);
  const improve = [];
  const leapfrog = [];
  for (const c of LIVE) {
    const board = [...FLOP, c];
    const hero = strength(HERO, board);
    const villain = villainBest(board, [...HERO, ...board]);
    if (hero > heroNow) improve.push(id(c));
    if (villain > hero && villain > villainNow) leapfrog.push(id(c));
  }
  const r = poker.exactVillainLeapfrogOutCounts(HERO, FLOP);
  assert.equal(improve.length, 47, 'every turn changes hero\'s best five from A-K-9-7-2');
  assert.deepEqual(Array.from(r.heroImproveDeckIndices), improve.sort((a, b) => a - b));
  assert.deepEqual(Array.from(r.leapfrogDeckIndices), leapfrog.sort((a, b) => a - b));
});

test('runout quantiles reject boards other than a flop, synchronously and asynchronously', async () => {
  const range = { indices: [id('Qh'), id('Qd')], weights: [1] };
  for (const board of [[], ['2c'], ['2c', '7d'], ['2c', '7d', '9s', 'Kd'], ['2c', '7d', '9s', 'Kd', '3h']]) {
    const msg = new RegExp(`need a 3-card flop \\(got ${board.length} board cards\\)`);
    assert.throws(() => poker.exactHeroEquityRunoutQuantiles(['Ah', 'Kd'], board), msg);
    assert.throws(() => poker.exactHeroEquityRunoutQuantiles(['Ah', 'Kd'], board, range), msg);
    await assert.rejects(poker.exactHeroEquityRunoutQuantilesAsync(['Ah', 'Kd'], board), msg);
    await assert.rejects(poker.exactHeroEquityRunoutQuantilesAsync(['Ah', 'Kd'], board, range), msg);
  }
  // A flop still works, and its mean is hero's exact equity against a random hand.
  const q = poker.exactHeroEquityRunoutQuantiles(['Ah', 'Kd'], ['2c', '7d', '9s']);
  assert.equal(q.n, 1081);
  near(q.mean, poker.exactHuEquityVsRandomHand(['Ah', 'Kd'], ['2c', '7d', '9s']), 1e-12, 'flop mean');
});
