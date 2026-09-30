'use strict';

// Correctness tests for the September 2026 fixes. Every assertion here either checks a
// hand-countable value, a published reference value, or a brute-force enumeration built on
// the legacy (sorted-group) evaluator, so a regression in the fast paths fails loudly.
// Run: npm test

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');

const poker = require(path.join(__dirname, '..'));

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';
const DECK = [];
for (const r of RANKS) for (const s of SUITS) DECK.push(r + s);

function near(actual, expected, eps, label) {
  assert.ok(
    Math.abs(actual - expected) <= eps,
    `${label}: expected ${expected}, got ${actual} (tolerance ${eps})`,
  );
}

function live(dead) {
  const d = new Set(dead);
  return DECK.filter((c) => !d.has(c));
}

// Deterministic PRNG so failures reproduce.
function lcg(seed) {
  let x = seed >>> 0;
  return () => {
    x = (Math.imul(x, 1664525) + 1013904223) >>> 0;
    return x / 4294967296;
  };
}

function sample(rng, pool, n) {
  const p = pool.slice();
  for (let i = 0; i < n; i++) {
    const j = i + Math.floor(rng() * (p.length - i));
    [p[i], p[j]] = [p[j], p[i]];
  }
  return p.slice(0, n);
}

// Legacy-evaluator brute force: hero equity vs a uniform random villain on a river board.
function bruteRiverEquityVsRandom(hero, board) {
  const pool = live([...hero, ...board]);
  const hs = poker.evaluateBestHand([...hero, ...board]).strength;
  let w = 0;
  let t = 0;
  let n = 0;
  for (let i = 0; i < pool.length; i++) {
    for (let j = i + 1; j < pool.length; j++) {
      const vs = poker.evaluateBestHand([pool[i], pool[j], ...board]).strength;
      if (hs > vs) w++;
      else if (hs === vs) t++;
      n++;
    }
  }
  return (w + 0.5 * t) / n;
}

test('fast evaluator agrees with the legacy evaluator on random 5/6/7-card hands', () => {
  const rng = lcg(20260907);
  for (let iter = 0; iter < 3000; iter++) {
    const boardLen = 3 + (iter % 3);
    const cards = sample(rng, DECK, 2 + boardLen);
    const hole = cards.slice(0, 2);
    const board = cards.slice(2);
    const fast = poker.evaluateHandStrengthFast(hole, board);
    const legacy = poker.evaluateHandStrength(hole, board);
    assert.equal(fast, legacy, `mismatch for ${hole.join('')} on ${board.join('')}`);
  }
});

test('fast evaluator finds a pair below a higher singleton', () => {
  assert.equal(
    poker.evaluateHandStrengthFast(['9h', '9d'], ['3h', '4c', 'Jh']),
    poker.evaluateHandStrength(['9h', '9d'], ['3h', '4c', 'Jh']),
  );
  assert.equal(poker.exactHuEquityVsKnownHand(['5h', '5d'], ['7c', '6c'], ['Ah', 'Kd', 'Qs', '9c', '8h']), 1);
  assert.equal(poker.exactHuEquityVsKnownHand(['Ah', '9d'], ['Kc', '9s'], ['9h', 'Jc', '4d', '3s', '2h']), 1);
});

test('exactHuEquityVsRandomHand handles a river board and matches brute force', () => {
  for (const [hero, board] of [
    [['5h', '5d'], ['Ah', 'Kd', 'Qs', '9c', '8h']],
    [['Ah', 'Kh'], ['Qh', 'Jh', '2c', '7d', '3s']],
    [['Jh', 'Jd'], ['Ah', 'Qd', '7s', '7c', '2h']],
  ]) {
    near(poker.exactHuEquityVsRandomHand(hero, board), bruteRiverEquityVsRandom(hero, board), 1e-12, hero.join(''));
  }
});

test('Monte Carlo equity lands near brute force on a river board', () => {
  const hero = ['5h', '5d'];
  const board = ['Ah', 'Kd', 'Qs', '9c', '8h'];
  const truth = bruteRiverEquityVsRandom(hero, board);
  near(poker.simulateHandOutcome(hero, board, 40000, 7, 1), truth, 0.02, 'simulateHandOutcome');
  near(poker.parallelHandSimulation(hero, board, 40000, 7, 1, 2), truth, 0.02, 'parallelHandSimulation');
});

test('exactHeroCategoryJointFlopToRiver survives a royal-flush flop and sums to 1', () => {
  const { jointMatrix } = poker.exactHeroCategoryJointFlopToRiver(['As', 'Ks'], ['Qs', 'Js', 'Ts']);
  assert.equal(jointMatrix.length, 81);
  near(Array.from(jointMatrix).reduce((a, b) => a + b, 0), 1, 1e-12, 'joint sums to 1');
  // Royal flush is folded into the straight-flush row/column (index 8); hero is a royal on every runout.
  near(jointMatrix[8 * 9 + 8], 1, 1e-12, 'royal stays royal');
});

test('Billings hand-potential example: AdQc on 3h4cJh vs all combos', () => {
  const range = new Float64Array(1326).fill(1);
  const b = poker.handPotentialBreakdown(['Ad', 'Qc'], ['3h', '4c', 'Jh'], range);
  assert.equal(b.nAhead, 628);
  assert.equal(b.nTied, 9);
  assert.equal(b.nBehind, 444);
  near(b.hs, (628 + 4.5) / 1081, 1e-12, 'HS');
  // Brute-force one-card PPot/NPot on the legacy evaluator with the same half-tie transitions.
  const hero = ['Ad', 'Qc'];
  const flop = ['3h', '4c', 'Jh'];
  const pool = live([...hero, ...flop]);
  const hp = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  const cls = (h, v) => (h > v ? 0 : h === v ? 1 : 2); // 0 ahead, 1 tied, 2 behind
  const hNow = poker.evaluateBestHand([...hero, ...flop]).strength;
  for (let i = 0; i < pool.length; i++) {
    for (let j = i + 1; j < pool.length; j++) {
      const vil = [pool[i], pool[j]];
      const now = cls(hNow, poker.evaluateBestHand([...vil, ...flop]).strength);
      for (const t of pool) {
        if (t === pool[i] || t === pool[j]) continue;
        const board = [...flop, t];
        const later = cls(
          poker.evaluateBestHand([...hero, ...board]).strength,
          poker.evaluateBestHand([...vil, ...board]).strength,
        );
        hp[now][later] += 1;
      }
    }
  }
  const behindMass = hp[2][0] + hp[2][1] + hp[2][2] + 0.5 * (hp[1][0] + hp[1][1] + hp[1][2]);
  const aheadMass = hp[0][0] + hp[0][1] + hp[0][2] + 0.5 * (hp[1][0] + hp[1][1] + hp[1][2]);
  const ppot = (hp[2][0] + 0.5 * hp[2][1] + 0.5 * hp[1][0]) / behindMass;
  const npot = (hp[0][2] + 0.5 * hp[0][1] + 0.5 * hp[1][2]) / aheadMass;
  near(b.ppot, ppot, 1e-9, 'PPot');
  near(b.npot, npot, 1e-9, 'NPot');
  near(b.ehs2, b.hs * b.hs * (1 - b.npot) + (1 - b.hs * b.hs) * b.ppot, 1e-12, 'EHS2 uses HS^2');
  const two = poker.handPotentialBreakdown(['Ad', 'Qc'], ['3h', '4c', 'Jh'], range, { streets: 2 });
  near(two.ppot, poker.twoStreetPositivePotential(['Ad', 'Qc'], ['3h', '4c', 'Jh'], range), 1e-12, 'streets:2');
});

test('call EV and breakeven equity share one pot convention', () => {
  near(poker.expectedValueCall(poker.breakevenCallEquity(90, 30), 90, 30), 0, 1e-12, 'EV at breakeven');
  near(poker.expectedValueCall(0.5, 10.9, 3.9), 0.5 * 10.9 - 0.5 * 3.9, 1e-12, 'non-integer args');
  near(poker.potOddsRatio(1.5, 1), 0.4, 1e-12, 'potOddsRatio keeps fractions');
  near(poker.multiwayExpectedValueCall(poker.breakevenCallEquity(90, 30), 90, 30, 0), 0, 1e-12, 'multiway k=0');
  near(poker.callOrFoldChipEvDelta(0.4, 100.5, 50.25), poker.expectedValueCall(0.4, 100.5, 50.25), 1e-12, 'alias');
});

test('rake variants reduce to their base at zero rake', () => {
  near(poker.breakevenCallEquityWithRake(100, 50, 0, 0), poker.breakevenCallEquity(100, 50), 1e-12, 'call');
  near(poker.expectedValueCallWithRake(0.25, 90, 30, 0, 0), 0, 1e-12, 'call EV at breakeven');
  near(poker.minimumDefenseFrequencyWithRake(100, 50, 0, 0), poker.minimumDefenseFrequency(100, 50), 1e-12, 'MDF');
  near(poker.bluffToValueRatioWithRake(100, 50, 0, 0), poker.bluffToValueRatio(100, 50), 1e-12, 'B2V');
  near(
    poker.expectedValueRaiseWithRake(0.4, 45, 80, 0.3, 185, 0, 0),
    poker.expectedValueRaise(0.4, 45, 80, 0.3, 185),
    1e-12,
    'raise EV',
  );
  near(poker.twoStreetPureBluffEvWithRake(100, 50, 75, 0.4, 0.5, 0, 0), poker.twoStreetPureBluffEv(100, 50, 75, 0.4, 0.5), 1e-12, 'two-street');
});

test('multi-street pure bluff EV and its breakeven solvers agree', () => {
  near(poker.twoStreetPureBluffEv(100, 50, 100, 0, 1), 150, 1e-12, 'call then fold');
  near(poker.twoStreetPureBluffEv(100, 50, 100, 0, 0), -150, 1e-12, 'called twice');
  near(poker.twoStreetPureBluffEv(100, 50, 100, 1, 0), 100, 1e-12, 'folds street 1');
  const f = poker.twoStreetPureBluffSameFoldEquity(100, 50, 100);
  near(poker.twoStreetPureBluffEv(100, 50, 100, f, f), 0, 1e-9, 'same-FE root');
  const fe2 = poker.breakevenFoldEquitySecondStreetPureBluff(100, 50, 100, 0.3);
  near(poker.twoStreetPureBluffEv(100, 50, 100, 0.3, fe2), 0, 1e-9, 'second-street root');
  const fe1 = poker.breakevenFoldEquityFirstStreetPureBluff(100, 50, 100, 0.4);
  near(poker.twoStreetPureBluffEv(100, 50, 100, fe1, 0.4), 0, 1e-9, 'first-street root');
  near(poker.threeStreetPureBluffEv(100, 50, 50, 50, 0, 0, 0), -150, 1e-12, 'three called');
  near(poker.threeStreetPureBluffEv(100, 50, 50, 50, 0, 0, 1), 200, 1e-12, 'fold on street 3');
  const f3 = poker.threeStreetPureBluffSameFoldEquity(100, 50, 50, 50);
  near(poker.threeStreetPureBluffEv(100, 50, 50, 50, f3, f3, f3), 0, 1e-9, 'three-street root');
});

test('tournament duel is gambler\'s ruin', () => {
  const a = poker.tournamentDuelAbsorptionProbabilities(1000, 9000, 0.5, 1000, 100);
  near(a.heroWinProbability, 0.1, 1e-12, 'short stack wins 10%');
  near(a.expectedHands, 9, 1e-9, 'expected duration h*v');
  near(a.heroPrizeEv, 10, 1e-9, 'prize EV');
  const b = poker.tournamentDuelAbsorptionProbabilities(9000, 1000, 0.5, 1000, 100);
  near(b.heroWinProbability, 0.9, 1e-12, 'big stack wins 90%');
  const c = poker.tournamentDuelAbsorptionProbabilities(1000, 1000, 0.6, 1000, 100);
  near(c.heroWinProbability, 0.6, 1e-12, 'one-flip duel');
});

test('polarized river indifference bet is the closed form', () => {
  const r = poker.solveRiverPolarizedIndifferenceBet(100, 90, 60);
  // v = 0.6 -> b* = (1 - v) P / (2v - 1) = 0.4 * 100 / 0.2 = 200
  near(r.betSize, 200, 1e-9, 'bet size');
  near(r.evAtIndifference, 0, 1e-9, 'villain indifferent');
  near(r.defenderMdf, 100 / 300, 1e-12, 'MDF at that size');
  near(r.bluffFrequency, 1, 1e-12, 'whole bluff supply bet');
  assert.equal(poker.solveRiverPolarizedIndifferenceBet(100, 40, 60).betSize, Infinity);
});

test('materializeVillainRangeAfterBlockers indexes the dense range correctly', () => {
  const r = poker.materializeVillainRangeAfterBlockers(new Float64Array(1326).fill(1), ['Ah', 'Kh'], ['Qs', '7c', '2d']);
  const weights = Array.from(r.weights1326 ?? r.weights);
  const sum = weights.reduce((a, b) => a + b, 0);
  near(sum, r.weightSum, 1e-9, 'weights sum to weightSum');
  assert.equal(weights.filter((w) => w > 0).length, r.liveComboCount);
  assert.equal(r.liveComboCount, (47 * 46) / 2);
});

test('PKO knockout matrix pays out every bounty except the winner\'s', () => {
  const stacks = [5000, 3000, 2000];
  const bounties = [20, 16, 12];
  const { matrix, n } = poker.pkoKnockoutProbabilityMatrix(stacks);
  let total = 0;
  for (let i = 0; i < n; i++) for (let j = 0; j < n; j++) total += matrix[i * n + j] * bounties[j];
  const winnerBounty = bounties.reduce((acc, b, j) => acc + b * (stacks[j] / 10000), 0);
  near(total, 48 - winnerBounty, 1e-9, 'sum of bounties minus expected winner bounty');
  for (let j = 0; j < n; j++) {
    let col = 0;
    for (let i = 0; i < n; i++) col += matrix[i * n + j];
    near(col, 1 - stacks[j] / 10000, 1e-12, `column ${j} = P(j busts)`);
  }
});

test('preflopCombosFromNotationMinusBlockers removes real combos', () => {
  assert.equal(poker.preflopCombosFromNotationMinusBlockers('AA', ['Ah']), 3);
  assert.equal(poker.preflopCombosFromNotationMinusBlockers('AA', ['Ah', 'Ad']), 1);
  // 3 live aces × 3 live kings = 9 pairs, minus the two still-suited pairs (clubs, spades).
  assert.equal(poker.preflopCombosFromNotationMinusBlockers('AKo', ['Ah', 'Kd']), 7);
  assert.equal(poker.preflopCombosFromNotationMinusBlockers('AKo', ['Ah']), 9);
  assert.equal(poker.preflopCombosFromNotationMinusBlockers('AKs', ['Ah']), 3);
  assert.equal(poker.preflopCombosFromNotationMinusBlockers('AKo', []), 12);
});

test('equityDeltaIfCardRemoved treats the card as dead in the runout too', () => {
  const hero = ['Ah', 'Kh'];
  const board = ['Qs', '7c', '2d', '5h'];
  const removed = 'Ac';
  const removedIndex = RANKS.indexOf(removed[0]) * 4 + SUITS.indexOf(removed[1]);
  const range = new Float64Array(1326).fill(1);
  const brute = (dead) => {
    const pool = live([...hero, ...board, ...dead]);
    let w = 0;
    let n = 0;
    for (let i = 0; i < pool.length; i++) {
      for (let j = i + 1; j < pool.length; j++) {
        for (const river of pool) {
          if (river === pool[i] || river === pool[j]) continue;
          const b = [...board, river];
          const hs = poker.evaluateBestHand([...hero, ...b]).strength;
          const vs = poker.evaluateBestHand([pool[i], pool[j], ...b]).strength;
          w += hs > vs ? 1 : hs === vs ? 0.5 : 0;
          n++;
        }
      }
    }
    return w / n;
  };
  near(poker.equityDeltaIfCardRemoved(hero, board, range, removedIndex), brute([removed]) - brute([]), 1e-9, 'delta');
  const grad = poker.exactEquityCardRemovalGradient(hero, board, range);
  near(grad.gradient[removedIndex], -(brute([removed]) - brute([])), 1e-9, 'gradient sign-flipped');
});

test('Omaha nuttedness excludes hero\'s own cards from the villain pool', () => {
  const hero = ['As', 'Kh', 'Qs', 'Jd'];
  const board = ['Ts', '9s', '8s', '2d', '3c'];
  const pool = live([...hero, ...board]);
  const hs = poker.evaluateOmahaBestHand(hero, board).strength;
  let better = 0;
  let total = 0;
  for (let a = 0; a < pool.length; a++)
    for (let b = a + 1; b < pool.length; b++)
      for (let c = b + 1; c < pool.length; c++)
        for (let d = c + 1; d < pool.length; d++) {
          total++;
          if (poker.evaluateOmahaBestHand([pool[a], pool[b], pool[c], pool[d]], board).strength > hs) better++;
        }
  near(poker.omahaNuttednessScore(hero, board), 1 - better / total, 1e-12, 'legal villain pool');
});

test('Omaha wrap outs ignore made straights and count a real 13-out wrap', () => {
  assert.deepEqual(poker.omahaWrapDrawOuts(['9h', 'Th', 'Jc', '2d'], ['7s', '8d', 'Kc']), { outs: 13, nutOuts: 13 });
  assert.deepEqual(poker.omahaWrapDrawOuts(['9h', 'Th', 'Jc', 'Qd'], ['6c', '7d', '8s']), { outs: 0, nutOuts: 0 });
});

test('razz kickers carry no ghost 13 on complete hands', () => {
  const e = poker.evaluateRazzHand(['2h', '2d', '3c', '4s', '5h']);
  assert.ok(!e.kickers.includes(13), `kickers ${e.kickers}`);
  const w = poker.evaluateRazzHand(['Ah', '2d', '3c', '4s', '5h', 'Kd', 'Kc']);
  assert.equal(w.rank, 'highCard');
});

test('exact stud equity enumerates the live pool when both hands are short', () => {
  const hero = ['Ah', 'Ad', 'Ac', 'Kh', 'Kd', '2c'];
  const villain = ['3h', '3d', '4c', '5s', '6h', '7d'];
  const pool = live([...hero, ...villain]);
  let w = 0;
  let n = 0;
  for (const h of pool) {
    const hs = poker.evaluateBestHand([...hero, h]).strength;
    for (const v of pool) {
      if (v === h) continue;
      const vs = poker.evaluateBestHand([...villain, v]).strength;
      w += hs > vs ? 1 : hs === vs ? 0.5 : 0;
      n++;
    }
  }
  near(poker.exactHuStudEquityVsKnown(hero, villain), w / n, 1e-12, 'stud 6v6');
});

test('exactMultiwayBestWorstRunout validates input before checking the board size', () => {
  assert.throws(() => poker.exactMultiwayBestWorstRunout([['Ah', 'Kh'], ['Ah', 'Qd']], ['2c', '7d', '9s', 'Ts', 'Jc']));
});

test('exactMultiwayWinTieLoseKnownHands is exported for n players', () => {
  const r = poker.exactMultiwayWinTieLoseKnownHands([['Ah', 'Ad'], ['Kh', 'Kd'], ['Qh', 'Qd']], ['2c', '7d', '9s', 'Ts', 'Jc']);
  assert.deepEqual(Array.from(r.win), [1, 0, 0]);
});

test('Nash heads-up solve uses ICM when payouts are supplied', () => {
  const chip = poker.nashHeadsUpJamCallSolve({ stackBb: 10, equityIterations: 40 });
  const icm = poker.nashHeadsUpJamCallSolve({ stackBb: 10, equityIterations: 40, otherStacks: [30, 30], payouts: [50, 30, 20, 0] });
  const mass = (v) => Array.from(v).reduce((a, b) => a + b, 0);
  assert.ok(mass(icm.jam) < mass(chip.jam), 'ICM pressure tightens the jam range');
});

// Before 4.0.0 a std::exception escaping a binding aborted the whole Node process
// (719 of 5,152 crash-sweep calls). These must now surface as ordinary JS errors.
test('invalid numeric input throws instead of terminating the process', () => {
  assert.throws(() => poker.spr(-1, 100));
  assert.throws(() => poker.harringtonM(0, 0, 0));
  assert.throws(() => poker.icmExpectedPayouts([], []));
  assert.throws(() => poker.wilsonScoreInterval(5, 0, 1.96));
});

test('Monte Carlo rejects impossible spots instead of reading past the deck', async () => {
  assert.throws(() => poker.simulateHandOutcome(['Ah'], [], 100, 1), /2 cards/);
  assert.throws(() => poker.simulateHandOutcome(['Ah', 'Kh'], ['2c', '3c', '4c', '5c', '6c', '7c'], 100, 1), /at most 5/);
  assert.throws(() => poker.simulateHandOutcome(['Ah', 'Kh'], ['Ah', '3c', '4c'], 100, 1), /duplicate/);
  assert.throws(() => poker.simulateHandOutcome(['Ah', 'Kh'], [], 100, 1, 30), /not enough cards/);
  assert.throws(() => poker.simulateHandOutcomeDetailed(['Ah'], [], 100, 1));
  assert.throws(() => poker.parallelHandSimulation(['Ah'], [], 100, 1, 1, 2));
  await assert.rejects(poker.simulateHandOutcomeAsync(['Ah'], [], 100, 1), /2 cards/);
});

test('typed-array ranges are length-checked and read from their own offset', () => {
  assert.throws(() => poker.materializeVillainRangeAfterBlockers(new Float64Array(0), ['Ah', 'Kh'], []), TypeError);
  assert.throws(
    () => poker.exactRangeDominatedComboFraction(['Ah', 'Kh'], ['2c', '7d', '9s'], { indices: new Uint8Array([0, 1, 2]), weights: [1, 1, 1] }),
    TypeError,
  );
  // A subarray() view must read its own 1326 weights, not the start of the shared buffer.
  const buf = new Float64Array(2 * 1326);
  const view = buf.subarray(1326);
  view.fill(1);
  const board = ['2c', '7d', '9s', 'Ts', 'Jc'];
  near(poker.exactHuEquityVsRange(['Ah', 'Kh'], board, view), poker.exactHuEquityVsRange(['Ah', 'Kh'], board, Float64Array.from(view)), 1e-12, 'subarray range');
});
