'use strict';

// Regression tests. Every assertion checks a hand-countable value, a published reference value,
// or a brute-force enumeration built on the legacy (sorted-group) evaluator, so a regression in
// the fast paths fails loudly. Run: npm test

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
  // Busting 4th pays nothing, so the caller risks more than chips: the call range must shrink.
  // (The jammer's range can widen in response, so it is not a stable thing to assert on.)
  assert.ok(mass(icm.call) < 0.85 * mass(chip.call), 'ICM tightens the calling range');
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

test('aborting an in-flight async call rejects with AbortError instead of crashing', async () => {
  const ac = new AbortController();
  const pending = poker.parallelHandSimulationAsync(['Ah', 'Kh'], ['Qh', 'Jh', '2c'], 20_000_000, 99, 1, 4, { signal: ac.signal });
  ac.abort();
  await assert.rejects(pending, (e) => e.name === 'AbortError');
  const queued = poker.simulateHandOutcomeAsync(['Ah', 'Kh'], [], 20_000_000, 1, 1, { signal: ac.signal });
  await assert.rejects(queued, (e) => e.name === 'AbortError'); // already-aborted signal
});

test('state APIs accept PKST bytes as well as objects', () => {
  const state = {
    players: [
      { holeCards: ['Ah', 'Kh'], stack: 200, seat: 0, committedThisStreet: 0 },
      { holeCards: ['7c', '7d'], stack: 180, seat: 1, committedThisStreet: 10 },
    ],
    communityCards: ['Qh', 'Jh', '2c'],
    phase: 'flop',
    pot: 30,
    currentBet: 10,
    smallBlind: 1,
    bigBlind: 2,
    actingIndex: 0,
    actedThisStreet: [false, true],
  };
  const bytes = poker.encodePokerState(state);
  const cfg = { monteCarloSimulations: 500, rngSeed: 1 };
  assert.equal(poker.decideAction(bytes, cfg, null, 0).action, poker.decideAction(state, cfg, null, 0).action);
  assert.deepEqual(poker.legalActionSummary(bytes), poker.legalActionSummary(state));
  assert.equal(poker.validatePokerState(bytes).valid, true);
});

test('pairwise bubble factor handles a pot that busts a player', () => {
  // Hero covers the short stack; losing the flip is fine, winning it busts seat 2 on the bubble.
  const bf = poker.icmPairwiseBubbleFactor([9000, 8500, 3000], [6000, 3600, 0], 0, 2, 3000);
  assert.ok(Number.isFinite(bf) && bf > 1, `bubble factor ${bf}`);
});

test('infinite pot odds render as the infinity sign on every platform', () => {
  assert.equal(poker.formatPotOdds(100, 0), '\u221e:1');
});

test('sprAfterCall uses the same pot convention as expectedValueCall', () => {
  // 90 in the middle (villain's 30 bet included), hero calls 30 with 270 behind: 240 / 120.
  assert.equal(poker.sprAfterCall(90, 30, 270), 2);
  assert.equal(poker.sprAfterCall(90, 30, 270), poker.sprAfterBet(60, 30, 270));
  near(poker.stackToPotAfterCall(90, 30, 270), 0.5, 1e-12, 'stackToPotAfterCall');
});

test('hands under five cards still count pairs, two pair, trips and quads', () => {
  const cases = [
    [['As', 'Kd'], 'highCard', [12, 11]],
    [['As', 'Ad'], 'onePair', [12]],
    [['7c', 'As', 'Ad'], 'onePair', [12, 5]],
    [['As', 'Ad', 'Kc', 'Kd'], 'twoPair', [12, 11]],
    [['Kd', 'As', 'Ad', 'Ac'], 'threeOfAKind', [12, 11]],
    [['As', 'Ad', 'Ac', 'Ah'], 'fourOfAKind', [12]],
  ];
  for (const [cards, rank, kickers] of cases) {
    const e = poker.evaluateBestHand(cards);
    assert.equal(e.rank, rank, cards.join(' '));
    assert.deepEqual(e.kickers.slice(0, kickers.length), kickers, cards.join(' '));
    assert.equal(poker.evaluateHandCategory(cards.slice(0, 2), cards.slice(2)), rank, cards.join(' '));
  }
  assert.equal(poker.evaluateShortDeckBestHand(['As', 'Ad']).rank, 'onePair');
  assert.equal(poker.evaluateShortDeckBestHand(['As', 'Ad', 'Ac', 'Kd']).rank, 'threeOfAKind');
  // A pocket pair outranks any unpaired two cards.
  assert.ok(
    poker.evaluateHandStrengthFast(['2c', '2d'], []) > poker.evaluateHandStrengthFast(['As', 'Kd'], []),
  );
});

test('fast and legacy evaluators agree on every two- and three-card hand', () => {
  const pack = (e) =>
    poker.handRankCategoryOrder(e.rank) * 2 ** 24 +
    e.kickers.reduce((acc, k, i) => acc + (k & 0x1f) * 2 ** (4 * (4 - i)), 0);
  const check = (cards) => {
    const legacy = pack(poker.evaluateBestHand(cards));
    const fast = poker.evaluateHandStrengthFast(cards.slice(0, 2), cards.slice(2));
    assert.equal(fast, legacy, cards.join(' '));
  };
  for (let a = 0; a < 52; a++) {
    for (let b = a + 1; b < 52; b++) {
      check([DECK[a], DECK[b]]);
      for (let c = b + 1; c < 52; c++) check([DECK[a], DECK[b], DECK[c]]);
    }
  }
});

test('detailed Monte Carlo SE uses the sampled payoff variance, not a Bernoulli formula', () => {
  // Everyone plays the royal-flush board: every trial pays exactly 1 / (villains + 1).
  for (const villains of [1, 2, 3]) {
    const r = poker.simulateHandOutcomeDetailed(['2c', '3d'], ['Ah', 'Kh', 'Qh', 'Jh', 'Th'], 100, 42, villains);
    near(r.estimate, 1 / (villains + 1), 1e-12, `${villains + 1}-way chop share`);
    assert.equal(r.se, 0, 'constant payoffs have zero sample variance');
    // Zero observed variance is not zero uncertainty: the interval keeps a Wilson width.
    assert.ok(r.ciLow < r.estimate && r.estimate < r.ciHigh, `interval ${r.ciLow}..${r.ciHigh}`);
  }
  const hu = poker.simulateHandOutcomeDetailed(['2c', '3d'], ['Ah', 'Kh', 'Qh', 'Jh', 'Th'], 100, 42, 1);
  const w = poker.wilsonScoreInterval(50, 100, 1.96);
  near(hu.ciLow, w.lower, 1e-12, 'integer mean matches wilsonScoreInterval');
  near(hu.ciHigh, w.upper, 1e-12, 'integer mean matches wilsonScoreInterval');

  // Variable payoffs (win 1, chop 0.5): population moments by enumerating every villain hand.
  const hero = ['Th', '3c'];
  const board = ['Ah', 'Kd', 'Qs', 'Jc', '2h'];
  const pool = live([...hero, ...board]);
  const hs = poker.evaluateBestHand([...hero, ...board]).strength;
  let m1 = 0;
  let m2 = 0;
  let count = 0;
  for (let i = 0; i < pool.length; i++) {
    for (let j = i + 1; j < pool.length; j++) {
      const vs = poker.evaluateBestHand([pool[i], pool[j], ...board]).strength;
      const x = hs > vs ? 1 : hs === vs ? 0.5 : 0;
      m1 += x;
      m2 += x * x;
      count++;
    }
  }
  const mu = m1 / count;
  const sd = Math.sqrt(m2 / count - mu * mu);
  const n = 20000;
  const r = poker.simulateHandOutcomeDetailed(hero, board, n, 7, 1);
  near(r.estimate, mu, 5 * sd / Math.sqrt(n), 'estimate');
  near(r.se * Math.sqrt(n), sd, 0.01, 'se is the sample SD over sqrt(n)');
  assert.ok(poker.monteCarloStandardError(r.estimate, n) > r.se * 1.3, 'Bernoulli SE overstates chop variance');

  const one = poker.simulateHandOutcomeDetailed(['Ah', 'Kd'], ['Qh', 'Jh', '2c'], 1, 42, 1);
  assert.equal(one.se, 0.5, 'one draw reports the largest SD a [0, 1] payoff can have');
  assert.throws(() => poker.simulateHandOutcomeDetailed(['Ah', 'Kd'], ['Qh', 'Jh', '2c'], 0, 42, 1), /at least 1/);
  // The exported Bernoulli helper keeps its meaning.
  near(poker.monteCarloStandardError(0.5, 100), 0.05, 1e-15, 'monteCarloStandardError');
});

// Heads-up flop: villain bet 100 (in the 110 pot) and is all-in; hero has 10 behind.
function shortCallState(heroCards = ['Ac', '2d']) {
  return {
    players: [
      { holeCards: heroCards, stack: 10, seat: 0, committedThisStreet: 0 },
      { holeCards: ['7c', '7d'], stack: 0, seat: 1, committedThisStreet: 100 },
    ],
    communityCards: ['Qh', 'Jh', 'Th'],
    phase: 'flop',
    pot: 110,
    currentBet: 100,
    smallBlind: 1,
    bigBlind: 2,
    actingIndex: 0,
    actedThisStreet: [false, true],
  };
}

test('validatePokerState checks objects and PKST bytes with the same rules', () => {
  const ok = shortCallState();
  assert.deepEqual(poker.validatePokerState(ok), { valid: true, errors: [] });
  const errors = (mutate) => {
    const s = shortCallState();
    mutate(s);
    return poker.validatePokerState(s).errors.join(' | ');
  };
  assert.match(errors((s) => { s.players[0].stack = -5; }), /players\[0\]\.stack must be non-negative/);
  assert.match(errors((s) => { s.communityCards = ['Ac', 'Jh', 'Th']; }), /card Ac appears twice/);
  assert.match(errors((s) => { s.players[1].seat = 0; }), /seat 0 is used twice/);
  assert.match(errors((s) => { s.actingIndex = 2; }), /actingIndex must be -1 or an index/);
  assert.match(errors((s) => { s.actedThisStreet = [false]; }), /one flag per player/);
  assert.match(errors((s) => { s.actedThisStreet = [0, 1]; }), /actedThisStreet\[0\] must be a boolean/);
  assert.match(errors((s) => { s.phase = 'turn'; }), /phase Turn needs 4 community cards/);
  assert.match(errors((s) => { s.pot = 50; }), /pot must include/);
  assert.match(errors((s) => { s.currentBet = 50; }), /currentBet must be at least/);
  assert.match(errors((s) => { s.players[0].holeCards = ['Ac']; }), /2 cards, or none/);
  // Chip fields are checked before conversion: no rounding, no 32-bit wraparound.
  assert.match(errors((s) => { s.pot = 110.5; }), /pot must be a finite integer/);
  assert.match(errors((s) => { s.pot = 1e12; }), /pot must fit in a 32-bit integer/);
  assert.match(errors((s) => { s.players[0].stack = NaN; }), /stack must be a finite integer/);
  assert.match(errors((s) => { s.players[0].stack = 2 ** 31 - 100; }), /must not exceed 2147483647 chips/);

  // PKST bytes go through the same semantic checks: write a negative stack into valid bytes.
  const bytes = poker.encodePokerState(ok);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const heroStackOffset = 8 + 8 * 4 + 1 + 3 + 2; // header, table fields, board, hero hole cards
  assert.equal(view.getInt32(heroStackOffset, true), 10);
  view.setInt32(heroStackOffset, -5, true);
  assert.match(poker.validatePokerState(bytes).errors.join(' | '), /players\[0\]\.stack must be non-negative/);
  assert.throws(() => poker.decideAction(bytes, { monteCarloSimulations: 10 }, null, 0), /non-negative/);

  // Unknown opponent hands are legitimate; PKST cannot store them and says so.
  const unknown = shortCallState();
  unknown.players[1].holeCards = [];
  assert.equal(poker.validatePokerState(unknown).valid, true);
  assert.throws(() => poker.encodePokerState(unknown), /unknown hands/);

  // A finished hand is a valid snapshot even though nobody can act on it.
  const done = { ...shortCallState(), phase: 'handcomplete', actingIndex: -1 };
  assert.equal(poker.validatePokerState(done).valid, true);
  assert.equal(poker.legalActionSummary(done).canCall, false);
  assert.throws(() => poker.decideAction(done, { monteCarloSimulations: 10 }, null, 0), /hand is over/);
});

test('decideAction prices a short all-in call on the chips hero can win', async () => {
  const state = shortCallState();
  const cfg = { monteCarloSimulations: 4000, rngSeed: 1 };
  // decideAction seeds its simulation with rngSeed ^ (pot * 1315423911), so the equity it saw
  // can be reproduced here.
  const equity = poker.simulateHandOutcome(['Ac', '2d'], state.communityCards, 4000, (1 ^ Math.imul(110, 1315423911)) >>> 0, 1);
  // 90 of villain's 100 is uncalled: hero risks 10 to win 10 dead + 10 matched.
  assert.ok(poker.expectedValueCall(equity, 20, 10) > 0, `equity ${equity} should call`);
  assert.ok(poker.expectedValueCall(equity, 110, 100) < 0, `raw pot and bet would fold ${equity}`);
  near(poker.expectedValueCall(0.4, 20, 10), 2, 1e-12, 'the documented +2 example');
  near(poker.expectedValueCall(0.4, 110, 100), -16, 1e-12, 'what the raw inputs give');

  assert.deepEqual(poker.decideAction(state, cfg, null, 0), { action: 'call', raiseBy: 0 });
  assert.deepEqual(await poker.decideActionAsync(state, cfg, null, 0), { action: 'call', raiseBy: 0 });
  const diag = poker.decideActionWithDiagnostics(state, cfg, null, 0);
  assert.equal(diag.decision.action, 'call');
  assert.equal(diag.legalActions.canRaise, false);
  const [row] = poker.runBotPolicyBatch([state], cfg);
  assert.equal(row.decision.action, 'call');
  assert.deepEqual(poker.decideAction(poker.encodePokerState(state), cfg, null, 0), { action: 'call', raiseBy: 0 });
});

test('decideAction has a safe contract on every entry point', async () => {
  // The old zero-simulation fallback read an encoded hand rank as equity and folded a royal flush.
  const royal = shortCallState(['Ah', 'Kh']);
  royal.players[0].stack = 1000;
  for (const sims of [0, -5]) {
    assert.throws(() => poker.decideAction(royal, { monteCarloSimulations: sims }, null, 0), /at least 1/);
    await assert.rejects(poker.decideActionAsync(royal, { monteCarloSimulations: sims }, null, 0), /at least 1/);
    assert.throws(() => poker.decideActionWithDiagnostics(royal, { monteCarloSimulations: sims }, null, 0), /at least 1/);
    assert.match(poker.runBotPolicyBatch([royal], { monteCarloSimulations: sims })[0].error, /at least 1/);
  }
  assert.equal(poker.normalizeBotConfig({ monteCarloSimulations: 0 }).monteCarloSimulations, 1);
  assert.notEqual(poker.decideAction(royal, { monteCarloSimulations: 50 }, null, 0).action, 'fold');

  // Raises stay inside the legal range however large the configured pot fraction.
  const open = shortCallState(['Ah', 'Kh']);
  Object.assign(open, { pot: 100, currentBet: 0, actedThisStreet: [false, false] });
  open.players[0].stack = 1000;
  open.players[1] = { holeCards: ['7c', '7d'], stack: 1000, seat: 1, committedThisStreet: 0 };
  const big = poker.decideAction(open, { monteCarloSimulations: 200, raisePotFraction: 50, aggressionThreshold: 0 }, null, 0);
  assert.deepEqual(big, { action: 'raise', raiseBy: 1000 });
  const legal = poker.legalActionSummary(open);
  assert.ok(legal.minRaiseTo <= big.raiseBy && big.raiseBy <= legal.maxRaiseTo);

  // Hero must be seated and able to act.
  assert.throws(() => poker.decideAction(open, { monteCarloSimulations: 10 }, null, 7), /heroSeat 7 is not at the table/);
  assert.throws(() => poker.decideAction(open, { monteCarloSimulations: 10 }, null, 0.5), /heroSeat must be an integer/);
  const allIn = shortCallState();
  assert.throws(() => poker.decideAction(allIn, { monteCarloSimulations: 10 }, null, 1), /all-in/);

  // Seeds reduce modulo 2^32 instead of overflowing.
  const cfg = { monteCarloSimulations: 300, aggressionThreshold: 0.6 };
  assert.deepEqual(
    poker.decideAction(open, { ...cfg, rngSeed: 2 ** 32 + 5 }, null, 0),
    poker.decideAction(open, { ...cfg, rngSeed: 5 }, null, 0),
  );

  // runBotPolicyBatch applies one opponent model per state and reports bad rows per row.
  const check = shortCallState(['Ac', '2d']);
  Object.assign(check, { pot: 20, currentBet: 0, actedThisStreet: [false, false] });
  check.players[1] = { holeCards: ['7c', '7d'], stack: 100, seat: 1, committedThisStreet: 0 };
  const tuning = { monteCarloSimulations: 2000, rngSeed: 3, opponentAggressionWeight: 1 };
  const aggressive = { aggressionFactor: 1 };
  const rows = poker.runBotPolicyBatch([check, check, { ...check, phase: 'showdown' }], tuning, [null, aggressive, null]);
  assert.equal(rows[0].decision.action, poker.decideAction(check, tuning, null, 0).action);
  assert.equal(rows[1].decision.action, poker.decideAction(check, tuning, aggressive, 0).action);
  assert.notEqual(rows[0].decision.action, rows[1].decision.action);
  assert.match(rows[2].error, /community cards/);
  assert.throws(() => poker.runBotPolicyBatch([check], tuning, aggressive), /opponentModels must be an array/);
});

test("Hold'em evaluation and exact equity reject duplicate cards like the variant APIs", () => {
  const board = ['Ah', '9h', '8d']; // hero's Ah is also on the board
  assert.throws(() => poker.evaluateBestHand(['Ah', 'Ah', '9h', '8d', '2c']), /duplicate/);
  assert.throws(() => poker.evaluateHandStrength(['Ah', 'Kd'], board), /duplicate/);
  assert.throws(() => poker.evaluateHandStrengthFast(['Ah', 'Kd'], board), /duplicate/);
  assert.throws(() => poker.evaluateHandCategory(['Ah', 'Kd'], board), /duplicate/);
  assert.throws(() => poker.exactHuEquityVsKnownHand(['Ah', 'Kd'], ['7c', '7d'], board), /duplicate/);
  assert.throws(() => poker.exactHuEquityVsRandomHand(['Ah', 'Kd'], board), /duplicate/);
  assert.throws(() => poker.exactHuEquityVsRange(['Ah', 'Kd'], board, new Float64Array(1326).fill(1)), /duplicate/);
  assert.throws(() => poker.exactHeroCategoryJointFlopToRiver(['Ah', 'Kd'], board), /duplicate/);
  assert.throws(() => poker.evaluateHandStrengthFastBatch(Uint8Array.of(50, 45), Uint8Array.of(50, 31, 26), 3), /duplicate/);
  assert.throws(() => poker.exactHuEquityVsRandomHandBatch(Uint8Array.of(50, 45), Uint8Array.of(50, 31, 26), 3), /duplicate/);
  // The variants already did this.
  assert.throws(() => poker.exactHuOmahaEquityVsKnown(['Ah', 'Kd', 'Qs', 'Jc'], ['7c', '7d', '6s', '5s'], board), /duplicate/);
});

test('opponent-modeling helpers validate their counts', () => {
  assert.deepEqual(poker.opponentFoldToCbetPosterior(2, 3, 4, 5), poker.betaBinomialFoldPosterior(2, 3, 4, 5));
  near(poker.opponentFoldToCbetPosterior(2, 3, 4, 5).posteriorMean, 6 / 14, 1e-15, 'Beta(6, 8) mean');
  assert.throws(() => poker.opponentFoldToCbetPosterior(2, 3, 4.5, 5), /integer counts/);
  near(poker.opponentAggressionFactor(3, 1, 2), 2, 0, '(bets + raises) / calls');
  near(poker.opponentAggressionFactor(3, 1, 0), 4, 0, 'calls floored at 1');
  assert.throws(() => poker.opponentAggressionFactor(-1, 1, 2), /non-negative/);
  assert.throws(() => poker.opponentRangeElasticityFromSizing([0.5, 1], [0.6]), /same length/);
});

test('simulateEquityVsRanges samples the joint range distribution', () => {
  const id = (c) => RANKS.indexOf(c[0]) * 4 + SUITS.indexOf(c[1]);
  const range = (...hands) => ({
    indices: Int32Array.from(hands.flatMap(([a, b]) => [id(a), id(b)])),
    weights: Float64Array.from(hands.map(() => 1)),
  });
  const within = (r, exact, label) => near(r.estimate, exact, 4.5 * r.se + 1e-12, label);

  // Villain 1 holds KsKh or 3c4d; villain 2 holds 4d5d or 9c9s, and 4d5d collides with 3c4d.
  // Card-disjoint tuples are (KK,45), (KK,99), (34,99), so P(villain 1 = KK) is 2/3. Drawing
  // villain 1 first and renormalising villain 2 would give 1/2 and a very different equity.
  const hero = ['Ac', 'Kc'];
  const flop = ['Kd', '7s', '2h'];
  const eq = (v1, v2) => poker.exactMultiwayEquityKnownHands([hero, v1, v2], flop)[0];
  const kk = ['Ks', 'Kh'];
  const low = ['3c', '4d'];
  const joint = (eq(kk, ['4d', '5d']) + eq(kk, ['9c', '9s']) + eq(low, ['9c', '9s'])) / 3;
  const sequential = 0.25 * eq(kk, ['4d', '5d']) + 0.25 * eq(kk, ['9c', '9s']) + 0.5 * eq(low, ['9c', '9s']);
  const r = poker.simulateEquityVsRanges(hero, flop, [range(kk, low), range(['4d', '5d'], ['9c', '9s'])], 60000, 11);
  within(r, joint, 'joint law');
  assert.ok(Math.abs(r.estimate - sequential) > 20 * r.se, 'not the sequential approximation');
  near(r.n / r.attempts, 0.75, 0.01, 'one tuple in four collides');

  // Singleton ranges reproduce exact known-hand equity, heads-up and multiway, with dead cards.
  within(poker.simulateEquityVsRanges(['Ah', 'Qd'], flop, [range(['7c', '7d'])], 20000, 2),
    poker.exactHuEquityVsKnownHand(['Ah', 'Qd'], ['7c', '7d'], flop), 'heads-up');
  const holes = [['Ah', 'Qd'], ['Jc', 'Tc'], ['8h', '8d'], ['6s', '5s']];
  const turn = ['Ks', '7c', '2d', '9h'];
  within(poker.simulateEquityVsRanges(holes[0], turn, holes.slice(1).map((h) => range(h)), 20000, 3),
    poker.exactMultiwayEquityKnownHands(holes, turn)[0], '4-way turn');
  within(poker.simulateEquityVsRanges(holes[0], ['Ks', '7c', '2d'], [range(holes[1]), range(holes[2])], 20000, 4, { deadCards: ['Td', '9c'] }),
    poker.exactMultiwayEquityWithDeadCards(holes.slice(0, 3), ['Ks', '7c', '2d'], ['Td', '9c'])[0], 'dead cards');

  // Fractional ties: everyone plays the royal board.
  for (const k of [1, 2]) {
    const chop = poker.simulateEquityVsRanges(['2c', '3d'], ['Ah', 'Kh', 'Qh', 'Jh', 'Th'],
      Array.from({ length: k }, () => new Float64Array(1326).fill(1)), 500, 5);
    near(chop.estimate, 1 / (k + 1), 1e-12, `${k + 1}-way chop`);
    assert.equal(chop.se, 0);
  }

  // Seeded, bounded, and explicit about impossible inputs.
  const args = [hero, flop, [range(kk, low), range(['4d', '5d'], ['9c', '9s'])], 2000];
  assert.deepEqual(poker.simulateEquityVsRanges(...args, 9), poker.simulateEquityVsRanges(...args, 9));
  assert.notDeepEqual(poker.simulateEquityVsRanges(...args, 9), poker.simulateEquityVsRanges(...args, 10));
  assert.throws(() => poker.simulateEquityVsRanges(hero, flop, [range(kk), range(kk)], 100, 1, { maxAttempts: 5000 }),
    /card-disjoint in 5000 attempts/);
  assert.throws(() => poker.simulateEquityVsRanges(hero, flop, [range(['Ac', 'Qd'])], 100, 1), /ranges\[0\]/);
  assert.throws(() => poker.simulateEquityVsRanges(hero, flop, [], 100, 1), /1 to 9/);
  assert.throws(() => poker.simulateEquityVsRanges(hero, flop, [range(kk)], 0, 1), /at least 1/);
});

test('range rankings order equal weights the same way on every platform', () => {
  const uniform = new Float64Array(1326).fill(1);
  assert.deepEqual(poker.rangeTopCombos(uniform, 3).map((c) => c.comboIndex), [0, 1, 2]);
  // Offsuit classes (12 combos) outweigh pairs and suited hands; ties keep first-seen order: 32o, 42o, ...
  assert.deepEqual(poker.rangeBucketWeightsByNotation(uniform).slice(0, 3).map((b) => b.notation), ['32o', '42o', '52o']);
});
