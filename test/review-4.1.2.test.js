'use strict';

// Regression checks for the corrections made before the 4.1.2 release, one test per correction.
// Each expected value is derived independently in the test, not read back from the package.

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');

const poker = require(path.join(__dirname, '..'));

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';
const DECK = [];
for (const r of RANKS) for (const s of SUITS) DECK.push(r + s);
const id = (c) => RANKS.indexOf(c[0]) * 4 + SUITS.indexOf(c[1]);
const comboIndex = (a, b) => {
  const lo = Math.min(a, b);
  const hi = Math.max(a, b);
  return lo * 51 - (lo * (lo - 1)) / 2 + (hi - lo - 1);
};
const sparse = (hands) => ({ indices: hands.flatMap(([a, b]) => [id(a), id(b)]), weights: hands.map(([, , w]) => w) });

function near(actual, expected, tol, label) {
  assert.ok(Math.abs(actual - expected) <= tol, `${label}: expected ${expected}, got ${actual}`);
}

test('1. a ragged equity matrix throws instead of reading past a row', () => {
  for (const fn of [
    () => poker.layeredPotChipEvFromEquities([100, 50], [[0.5, 0.5], []]),
    () => poker.layeredPotChipEvFromEquities([100, 50], [[0.5, 0.5], [0.5]]),
    () => poker.layeredPotChipEvFromEquitiesWithRake([100, 50], [[0.5, 0.5], []], 0.05, 3),
  ]) {
    assert.throws(fn, /equity row 1 has \d entries; every row needs one per pot layer \(2\)/);
  }
  assert.deepEqual(poker.layeredPotChipEvFromEquities([100, 50], [[0.5, 1], [0.5, 0]]), [100, 50]);
});

test('2. decideAction prices each side-pot layer against its own eligible opponents', () => {
  // Royal flush on board: everyone ties. Commitments [10, 1, 100]; hero calls 90 with 90 behind.
  // Main pot 3 chips three ways (hero gets 1), side pot 198 two ways (hero gets 99): +10 for 90.
  const state = {
    players: [
      { holeCards: ['2c', '3d'], stack: 90, seat: 0, committedThisStreet: 10 },
      { holeCards: [], stack: 0, seat: 1, committedThisStreet: 1 },
      { holeCards: [], stack: 500, seat: 2, committedThisStreet: 100 },
    ],
    communityCards: ['Ah', 'Kh', 'Qh', 'Jh', 'Th'],
    phase: 'river', pot: 111, currentBet: 100, actingIndex: 0, actedThisStreet: [false, true, true],
  };
  for (const villains of [1, 2, 3]) {
    const d = poker.decideAction(state, { monteCarloSimulations: 300, monteCarloVillains: villains, rngSeed: 3 }, null, 0);
    assert.equal(d.action, 'call', `monteCarloVillains ${villains}`);
  }
  // Same spot with every earlier-street chip recorded per player gives the same answer.
  const withTotals = structuredClone(state);
  withTotals.pot = 141;
  for (const [i, extra] of [[0, 10], [1, 10], [2, 10]]) {
    withTotals.players[i].totalCommittedHand = withTotals.players[i].committedThisStreet + extra;
  }
  assert.equal(poker.decideAction(withTotals, { monteCarloSimulations: 300, monteCarloVillains: 2, rngSeed: 3 }, null, 0).action, 'call');
});

test('3. the pure-bluff rake threshold wins the pot, not the pot plus hero\'s returned bet', () => {
  near(poker.breakevenFoldEquityPureBluffWithRake(100, 50, 0, 0), 1 / 3, 1e-15, 'no rake');
  near(poker.breakevenFoldEquityPureBluffWithRake(100, 50, 0, 0), poker.breakevenFoldEquityPureBluff(100, 50), 1e-15, 'matches the rake-free form');
  for (const [pot, bet, rf, cap] of [[100, 50, 0.05, 1000], [60, 45, 0.1, 4], [200, 10, 0.05, 3]]) {
    const fe = poker.breakevenFoldEquityPureBluffWithRake(pot, bet, rf, cap);
    near(poker.expectedValueRaiseWithRake(0, pot, bet, fe, pot + 2 * bet, rf, cap), 0, 1e-12, `EV at ${fe}`);
  }
});

test('4. the semi-bluff rake threshold rakes the fold branch like expectedValueRaiseWithRake', () => {
  // Fold: 100 - 5% of 150 = 92.5. Called: 0.1 * (200 - 10) - 50 = -31. 31 / 123.5.
  const fe = poker.breakevenFoldEquitySemiBluffWithRake(100, 50, 0.1, 200, 0.05, 1000);
  near(fe, 31 / 123.5, 1e-15, 'threshold');
  near(poker.expectedValueRaiseWithRake(0.1, 100, 50, fe, 200, 0.05, 1000), 0, 1e-12, 'EV at the threshold');
  near(poker.breakevenFoldEquitySemiBluffWithRake(100, 50, 0.1, 200, 0, 0), poker.breakevenFoldEquitySemiBluff(100, 50, 0.1, 200), 1e-15, 'no rake');
  assert.equal(poker.breakevenFoldEquitySemiBluffWithRake(100, 50, 0.5, 200, 0.05, 1000), 0, 'called branch already profitable');
});

test('5. implied odds: the extra winnings a call needs, inverse of the required-equity helper', () => {
  assert.equal(poker.impliedBreakevenFutureWin(100, 50, 0.25), 50);
  // Without those 50 chips the call loses 12.5; with them it breaks even.
  near(poker.expectedValueCall(0.25, 100, 50), -12.5, 1e-12, 'EV now');
  near(poker.expectedValueCall(0.25, 150, 50), 0, 1e-12, 'EV with the future win');
  for (const [pot, call, e] of [[100, 50, 0.25], [30, 70, 0.2], [500, 20, 0.01]]) {
    const w = poker.impliedBreakevenFutureWin(pot, call, e);
    assert.ok(w > 0);
    near(poker.impliedOddsRequiredEquityFromFutureWin(pot, call, w), e, 1e-12, `inverse at ${e}`);
  }
  assert.equal(poker.impliedBreakevenFutureWin(100, 50, 0.5), 0, 'already profitable');
  assert.equal(poker.impliedBreakevenFutureWin(100, 50, 0), Infinity);
});

// Independent predictive runout weights: each runout weighs the range mass that avoids its cards.
function runouts(hero, board, hands) {
  const used = new Set([...hero, ...board]);
  const live = DECK.filter((c) => !used.has(c));
  const out = [];
  const need = 5 - board.length;
  const walk = (start, picked) => {
    if (picked.length === need) {
      const mass = hands.filter(([a, b]) => !picked.includes(a) && !picked.includes(b) && !used.has(a) && !used.has(b))
        .reduce((s, [, , w]) => s + w, 0);
      if (mass > 0) out.push({ board: [...board, ...picked], mass });
      return;
    }
    for (let i = start; i < live.length; i++) walk(i + 1, [...picked, live[i]]);
  };
  walk(0, []);
  return out;
}

test('6. information regret compares the best call-or-fold decision on both sides', () => {
  const hero = ['8h', '6h']; // open-ended: a 5 or a T makes a straight
  const board = ['2c', '7d', '9s', 'Kd'];
  const hands = [['9c', '9d', 1], ['Ac', 'Kc', 2], ['6c', '6d', 1]];
  const ev = (e, pot, call) => e * pot - (1 - e) * call;
  const values = [];
  for (const [pot, call] of [[100, 50], [10, 100], [300, 20], [100, 30]]) {
    const now = poker.exactHuEquityVsRange(hero, board, sparse(hands));
    let num = 0;
    let den = 0;
    for (const r of runouts(hero, board, hands)) {
      num += r.mass * Math.max(0, ev(poker.exactHuEquityVsRange(hero, r.board, sparse(hands)), pot, call));
      den += r.mass;
    }
    const expected = Math.max(0, num / den - Math.max(0, ev(now, pot, call)));
    near(poker.exactInformationRegretVsClairvoyant(hero, board, sparse(hands), pot, call), expected, 1e-9, `pot ${pot}, call ${call}`);
    values.push(expected);
  }
  assert.ok(values.some((v) => v > 1), `some spot has real information value: ${values}`);
  // No river makes the call profitable (4h5h is drawing dead): seeing it first is worth nothing.
  assert.equal(poker.exactInformationRegretVsClairvoyant(['4h', '5h'], board, sparse(hands), 10, 100), 0);
  assert.equal(poker.exactInformationRegretVsClairvoyant(hero, [...board, '3s'], sparse(hands), 100, 50), 0, 'river');
});

test('7. range runout statistics weight boards by the range, so their mean is the direct equity', () => {
  const hero = ['Ah', 'Ad'];
  const flop = ['2c', '7d', '9s'];
  const hands = [['Kh', 'Kd', 1], ['9c', '9d', 9], ['3c', '4d', 1]];
  const direct = poker.exactHuEquityVsRange(hero, flop, sparse(hands));
  near(direct, 0.24086317722681358, 1e-15, 'direct equity');
  const q = poker.exactHeroEquityRunoutQuantiles(hero, flop, sparse(hands));
  near(q.mean, direct, 1e-12, 'predictive mean');
  const rs = runouts(hero, flop, hands);
  assert.equal(q.n, rs.length);
  // A singleton range skips the runouts it cannot coexist with instead of throwing.
  const kings = poker.exactHeroEquityRunoutQuantiles(hero, flop, sparse([['Kh', 'Kd', 1]]));
  assert.equal(kings.n, 990); // C(45, 2): the two kings are out of the 47 live cards
  near(kings.mean, poker.exactHuEquityVsRange(hero, flop, sparse([['Kh', 'Kd', 1]])), 1e-12, 'singleton mean');
  // One random opponent: every runout is equally likely, and the results are unchanged from 4.1.1.
  assert.deepEqual(poker.exactHeroEquityRunoutQuantiles(['Ah', 'Kd'], flop), {
    mean: 0.5230954316523226, variance: 0.04872675058448483, p05: 0.27121212121212124,
    p50: 0.4813131313131313, p95: 0.898989898989899, n: 1081,
  });
});

test('8. removing a card that empties the range is undefined, not an equity of 0', () => {
  const kings = sparse([['Kh', 'Kd', 1]]);
  const river = ['2c', '7d', '9s', 'Qh', '3s'];
  const s = poker.rangeRemovalSensitivityVsHero(['Ah', 'Ad'], river, kings);
  assert.ok(Number.isNaN(s[id('Kh')]) && Number.isNaN(s[id('Kd')]));
  assert.equal(s[id('Qs')], 0);
  assert.equal(s[id('Ah')], 0, 'hero card');
  const g = poker.exactEquityCardRemovalGradient(['Ah', 'Ad'], river.slice(0, 4), kings);
  assert.ok(Number.isNaN(g.gradient[id('Kh')]));
  // Other failures are reported, not turned into zeros.
  assert.throws(() => poker.rangeRemovalSensitivityVsHero(['Ah', 'Ah'], river, kings), /duplicate/);
  assert.throws(() => poker.rangeRemovalSensitivityVsHero(['Ah', 'Ad'], river, sparse([['Ah', 'Kd', 1]])), /no valid combos/);
});

test('9. bluff-to-value ratios are ratios of bluffs to value bets', () => {
  // Pot-sized bet: bluffs are 1/3 of bets, so 1 bluff per 2 value bets.
  assert.equal(poker.bluffToValueRatio(100, 100), 0.5);
  assert.equal(poker.valueToBluffRatio(100, 100), 2);
  near(poker.bluffToValueRatio(100, 50), 1 / 3, 1e-15, 'half pot');
  // The bluff share of bets (the old return value) is the caller's breakeven equity.
  const share = poker.breakevenCallEquity(200, 100);
  near(share / (1 - share), poker.bluffToValueRatio(100, 100), 1e-15, 'share to ratio');
  assert.equal(poker.bluffToValueRatioWithRake(100, 100, 0, 0), 0.5);
  const raked = poker.breakevenCallEquityWithRake(200, 100, 0.05, 3);
  near(raked / (1 - raked), poker.bluffToValueRatioWithRake(100, 100, 0.05, 3), 1e-15, 'raked share to ratio');
  near(poker.valueToBluffRatioWithRake(100, 100, 0.05, 3), 1 / poker.bluffToValueRatioWithRake(100, 100, 0.05, 3), 1e-15, 'inverse');
  assert.equal(poker.bluffToValueRatioWithRake(10, 100, 1, 1000), Infinity, 'the rake leaves the caller nothing');
  assert.equal(poker.valueToBluffRatioWithRake(10, 100, 1, 1000), 0);
  assert.equal(poker.bluffToValueRatio(100, 0), 0);
  assert.equal(poker.valueToBluffRatio(100, 0), Infinity);
});

test('10. a tiny bet frequency still conditions the defender on the hands that bet', () => {
  const board = ['2c', '7d', '9s', 'Jc', 'Qh'];
  const villain = sparse([['9c', '9d', 1], ['3c', '4d', 1]]); // a set and air, weight 1 (mantissa 0.5)
  const hero = sparse([['Ah', 'Jd', 1]]); // a bluff catcher
  const mix = (f) => {
    const m = new Float64Array(1326);
    m[comboIndex(id('9c'), id('9d'))] = f; // only the set ever bets
    return m;
  };
  const expected = poker.bestResponseRiver(100, 50, hero, villain, board, mix(0.5));
  assert.equal(expected.action, 'fold');
  for (const f of [5e-324, 1e-310, 1e-300]) {
    assert.deepEqual(poker.bestResponseRiver(100, 50, hero, villain, board, mix(f)), expected, `bet frequency ${f}`);
  }
});
