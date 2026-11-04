'use strict';

// Weighted-range consistency: a range is a distribution, so its overall scale, its encoding
// (sparse or dense), the order of opponents, and a consistent suit relabeling must not change any
// result. Every check is deterministic (fixed seeds, fixed spots) and small.

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

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

function near(actual, expected, tol, label) {
  assert.ok(Math.abs(actual - expected) <= tol, `${label}: expected ${expected}, got ${actual} (tolerance ${tol})`);
}

/** [[cardA, cardB, weight], ...] as a dense Float64Array(1326). */
function dense(hands, scale = 1) {
  const d = new Float64Array(1326);
  for (const [a, b, w] of hands) d[comboIndex(id(a), id(b))] = w * scale;
  return d;
}
/** The same hands as a sparse spec, in dense (lexicographic) order so sampling sees the same list. */
function sparse(hands, scale = 1) {
  const sorted = [...hands].sort((x, y) => comboIndex(id(x[0]), id(x[1])) - comboIndex(id(y[0]), id(y[1])));
  return {
    indices: Int32Array.from(sorted.flatMap(([a, b]) => [id(a), id(b)])),
    weights: Float64Array.from(sorted.map(([, , w]) => w * scale)),
  };
}

const HERO = ['Ah', 'Kd'];
const FLOP = ['Qs', '7c', '2d'];
const TURN = [...FLOP, '9h'];
const RIVER = [...TURN, '3s'];
// Unequal weights, so a wrong normalization shows up.
const BASE = poker.rangeFromNotationWeights([
  { notation: 'QQ', weight: 3 },
  { notation: 'AKs', weight: 1 },
  { notation: '76s', weight: 2 },
  { notation: 'T9o', weight: 0.5 },
]);
const scaled = (s) => Float64Array.from(BASE, (w) => w * s);
// Packed combos avoid the flop (Qs 7c 2d): 3c 4d 5h 6s and 3h 4s 6c 7d (plus 8c / 8h for Big O).
const OMAHA = (s) => ({ packed: Uint8Array.of(4, 9, 14, 19, 6, 11, 16, 21), weights: [s, 2 * s] });
const BIG_O = (s) => ({ packed: Uint8Array.of(4, 9, 14, 19, 24, 6, 11, 16, 21, 26), weights: [s, 2 * s] });
const SHORT = (s) => {
  const a = new Float64Array(81);
  a[80] = 3 * s;
  a[70] = s;
  return a;
};

let buckets;
const BUCKETS = () => (buckets ??= poker.ehs2BucketsVsRange(FLOP, SMALL(1)));
// The 1326-combo tables cost range size × runouts; three weighted combos keep them quick.
function SMALL(s) {
  return dense([['Qh', 'Qd', 3], ['7d', '6d', 2], ['Tc', '9d', 0.5]], s);
}

// One call per export that takes a weighted range; `s` scales every weight (and any absolute
// weight threshold that goes with it).
const CASES = {
  exactHuEquityVsRange: (s) => poker.exactHuEquityVsRange(HERO, RIVER, scaled(s)),
  handStrengthVsRange: (s) => poker.handStrengthVsRange(HERO, FLOP, scaled(s)),
  positivePotentialVsRange: (s) => poker.positivePotentialVsRange(HERO, FLOP, scaled(s)),
  negativePotentialVsRange: (s) => poker.negativePotentialVsRange(HERO, FLOP, scaled(s)),
  effectiveHandStrength: (s) => poker.effectiveHandStrength(HERO, FLOP, scaled(s)),
  effectiveHandStrengthSquared: (s) => poker.effectiveHandStrengthSquared(HERO, FLOP, scaled(s)),
  handPotentialBreakdown: (s) => poker.handPotentialBreakdown(HERO, FLOP, scaled(s)),
  twoStreetPositivePotential: (s) => poker.twoStreetPositivePotential(HERO, FLOP, scaled(s)),
  twoStreetNegativePotential: (s) => poker.twoStreetNegativePotential(HERO, FLOP, scaled(s)),
  comboEhsTableVsRange: (s) => poker.comboEhsTableVsRange(FLOP, SMALL(s)),
  ehs2BucketsVsRange: (s) => poker.ehs2BucketsVsRange(FLOP, SMALL(s)),
  bucketMassFromRange: (s) => poker.bucketMassFromRange(scaled(s), BUCKETS(), 20),
  simulateEquityVsRange: (s) => poker.simulateEquityVsRange(HERO, FLOP, scaled(s), 3000, 7),
  simulateEquityVsRanges: (s) => poker.simulateEquityVsRanges(HERO, FLOP, [scaled(s), scaled(s)], 3000, 7),
  equityDeltaIfCardRemoved: (s) => poker.equityDeltaIfCardRemoved(HERO, TURN, scaled(s), id('9c')),
  materializeVillainRangeAfterBlockers: (s) => poker.materializeVillainRangeAfterBlockers(scaled(s), HERO, FLOP),
  exactInformationRegretVsClairvoyant: (s) => poker.exactInformationRegretVsClairvoyant(HERO, TURN, scaled(s), 100, 20),
  exactRangeDominatedComboFraction: (s) => poker.exactRangeDominatedComboFraction(HERO, RIVER, scaled(s)),
  exactHeroEquityRunoutQuantiles: (s) => poker.exactHeroEquityRunoutQuantiles(HERO, FLOP, scaled(s)),
  exactEquityCardRemovalGradient: (s) => poker.exactEquityCardRemovalGradient(HERO, TURN, scaled(s)),
  normalizeSparseRange: (s) => poker.normalizeSparseRange(scaled(s)),
  pruneRangeByMinWeight: (s) => poker.pruneRangeByMinWeight(scaled(s), 0.004 * s),
  mergeSparseRanges: (s) => poker.mergeSparseRanges(scaled(s), dense([['As', 'Ad', 1]], s), 0.3, 0.7),
  intersectSparseRanges: (s) => poker.intersectSparseRanges(scaled(s), scaled(s)),
  subtractSparseRange: (s) => poker.subtractSparseRange(scaled(s), dense([['Qh', 'Qd', 0.01]], s)),
  rangeComboCount: (s) => poker.rangeComboCount(scaled(s), 0.004 * s),
  rangeShannonEntropy: (s) => poker.rangeShannonEntropy(scaled(s)),
  rangeGiniCoefficient: (s) => poker.rangeGiniCoefficient(scaled(s)),
  rangeCoverageFraction: (s) => poker.rangeCoverageFraction(scaled(s)),
  rangeWeightTopKMass: (s) => poker.rangeWeightTopKMass(scaled(s), 6),
  rangeDistanceL1: (s) => poker.rangeDistanceL1(scaled(s), dense([['As', 'Ad', 1]], s)),
  rangeDistanceL2: (s) => poker.rangeDistanceL2(scaled(s), dense([['As', 'Ad', 1]], s)),
  rangeDistanceJensenShannon: (s) => poker.rangeDistanceJensenShannon(scaled(s), dense([['As', 'Ad', 1]], s)),
  rangeCosineSimilarity: (s) => poker.rangeCosineSimilarity(scaled(s), dense([['Qh', 'Qd', 1]], s)),
  rangeTopCombos: (s) => poker.rangeTopCombos(scaled(s), 5),
  rangeBucketWeightsByHandClass: (s) => poker.rangeBucketWeightsByHandClass(scaled(s)),
  rangeBucketWeightsByNotation: (s) => poker.rangeBucketWeightsByNotation(scaled(s)),
  rangeBlockerPressureByCard: (s) => poker.rangeBlockerPressureByCard(scaled(s)),
  rangeRemovalSensitivityVsHero: (s) => poker.rangeRemovalSensitivityVsHero(HERO, RIVER, scaled(s)),
  blockerMatrixByCard: (s) => poker.blockerMatrixByCard(scaled(s)),
  villainPolarizedRangeScore: (s) => poker.villainPolarizedRangeScore(scaled(s), FLOP),
  cfrRiverBetCallFoldSolve: (s) => poker.cfrRiverBetCallFoldSolve(100, 50, scaled(s), scaled(s), RIVER, 60),
  bestResponseRiver: (s) => poker.bestResponseRiver(100, 50, scaled(s), scaled(s), RIVER, 0.5),
  exploitabilityRiver: (s) => poker.exploitabilityRiver(100, 50, scaled(s), scaled(s), RIVER, 0.5, 0.5),
  fictitiousPlayRiver: (s) => poker.fictitiousPlayRiver(100, 50, scaled(s), scaled(s), RIVER, 60),
  evOfStrategyProfile: (s) => poker.evOfStrategyProfile(100, 50, scaled(s), scaled(s), RIVER, 0.5, 0.5),
  solveHuRiverCheckBetTree: (s) => poker.solveHuRiverCheckBetTree(100, 50, scaled(s), scaled(s), RIVER, 60),
  applySuitPermToRange1326: (s) => poker.applySuitPermToRange1326(scaled(s), [1, 2, 3, 0]),
  simulateOmahaEquityVsRange: (s) => poker.simulateOmahaEquityVsRange(['Ah', 'Kd', 'Jh', 'Tc'], FLOP, OMAHA(s), 400, 3),
  simulateBigOEquityVsRange: (s) => poker.simulateBigOEquityVsRange(['Ah', 'Kd', 'Jh', 'Tc', '9s'], FLOP, BIG_O(s), 400, 3),
  simulateShortDeckEquityVsRange: (s) => poker.simulateShortDeckEquityVsRange(['Ah', 'Kd'], ['Qs', '7c', '8d'], SHORT(s), 400, 3),
};

/** Numeric leaves of a result, with their paths. */
function leaves(v, key = '') {
  if (typeof v === 'number') return [[key, v]];
  if (v === null || typeof v !== 'object') return [];
  const entries = ArrayBuffer.isView(v) ? Array.from(v).map((x, i) => [i, x]) : Object.entries(v);
  return entries.flatMap(([k, x]) => leaves(x, key ? `${key}.${k}` : String(k)));
}

test('every export that takes a weighted range is covered here', async () => {
  const { parseSignatures } = await import(pathToFileURL(path.join(__dirname, '..', 'scripts', 'crash-sweep', 'signatures.mjs')).href);
  const sigs = parseSignatures(path.join(__dirname, '..', 'index.d.ts'));
  const takesRange = [...sigs]
    .filter(([, params]) => params.some((p) => /range/i.test(p.name) || /RangeSpec|RangeInput/.test(p.type)))
    .map(([name]) => name);
  assert.ok(takesRange.length >= 50, `found ${takesRange.length}`);
  for (const name of takesRange) {
    const sync = name.replace(/Async$/, '');
    assert.ok(CASES[sync], `${name} has no scale case (async forms share their sync implementation)`);
  }
});

// Powers of two scale exactly in binary floating point, so every output must be either unchanged
// or multiplied by exactly s. 2^±600 lies beyond the internal rescaling threshold (2^±256); 2^100
// does not. Results are compared field by field.
test('scaling every weight preserves the represented distribution', () => {
  const failures = [];
  for (const [name, call] of Object.entries(CASES)) {
    const base = leaves(call(1));
    assert.ok(base.length > 0, `${name} returned no numbers`);
    for (const s of [2 ** 100, 2 ** 600, 2 ** -600]) {
      let got;
      try {
        got = leaves(call(s));
      } catch (e) {
        failures.push(`${name} at 2^${Math.log2(s)}: threw ${e.message}`);
        continue;
      }
      if (got.length !== base.length) {
        failures.push(`${name} at 2^${Math.log2(s)}: ${got.length} values, expected ${base.length}`);
        continue;
      }
      base.forEach(([k, x], i) => {
        const y = got[i][1];
        const tol = 1e-12 * (1 + Math.abs(x));
        const same = Math.abs(y - x) <= tol;
        const inUnits = Number.isFinite(y) && Math.abs(y / s - x) <= tol;
        if (!same && !inUnits) failures.push(`${name}.${k} at 2^${Math.log2(s)}: ${x} became ${y}`);
      });
    }
  }
  assert.deepEqual(failures, []);
});

test('the reported overflow: [1, 1] and [1e308, 1e308] are the same range', () => {
  const two = [['Qh', 'Qd', 1], ['5c', '4c', 1]];
  for (const s of [1, 1e308, 1e-308]) {
    near(poker.exactHuEquityVsRange(HERO, RIVER, dense(two, s)), 0.5, 0, `dense ×${s}`);
    near(poker.exactHuEquityVsRange(HERO, RIVER, sparse(two, s)), 0.5, 0, `sparse ×${s}`);
  }
  // A Monte Carlo draw over the same seed picks the same hands.
  const mc = (s) => poker.simulateEquityVsRange(HERO, FLOP, dense(two, s), 4000, 11);
  assert.equal(mc(1e308), mc(1));
  // Totals reported in the caller's units (3e308 here) cannot be represented: rejected, never
  // returned as Infinity. Scale-free results from the same range still work.
  const queens = [['Qh', 'Qd', 1], ['Qh', 'Qc', 1], ['Qd', 'Qc', 1]];
  const huge = dense(queens, 1e308);
  assert.throws(() => poker.materializeVillainRangeAfterBlockers(huge, HERO, FLOP), /too large/);
  assert.throws(() => poker.handPotentialBreakdown(HERO, FLOP, huge), /too large/);
  assert.throws(() => poker.rangeBucketWeightsByHandClass(huge), /too large/);
  assert.throws(() => poker.rangeBlockerPressureByCard(huge), /too large/);
  near(poker.handStrengthVsRange(HERO, FLOP, huge), poker.handStrengthVsRange(HERO, FLOP, dense(queens)), 0, 'ratios still work');
  near(poker.exactHuEquityVsRange(HERO, RIVER, huge), poker.exactHuEquityVsRange(HERO, RIVER, dense(queens)), 0, 'equity still works');
  // A NaN, infinite, or negative weight has no distribution to represent.
  for (const bad of [NaN, Infinity, -1]) {
    const d = dense(two);
    d[comboIndex(id('Qh'), id('Qd'))] = bad;
    assert.throws(() => poker.exactHuEquityVsRange(HERO, RIVER, d), /finite and non-negative/, `exact ${bad}`);
    assert.throws(() => poker.simulateEquityVsRange(HERO, FLOP, d, 100, 1), /finite and non-negative/, `mc ${bad}`);
    assert.throws(() => poker.rangeShannonEntropy(d), /finite and non-negative/, `tools ${bad}`);
    assert.throws(() => poker.exactHuEquityVsRange(HERO, RIVER, { indices: [id('Qh'), id('Qd')], weights: [bad] }), /finite and non-negative/);
    assert.throws(() => poker.simulateOmahaEquityVsRange(['Ah', 'Kd', 'Jh', 'Tc'], FLOP, { packed: Uint8Array.of(4, 9, 14, 19), weights: [bad] }, 10, 1), /finite and non-negative/);
  }
});

test('sparse and dense representations give the same results', () => {
  const hands = [['Qh', 'Qd', 3], ['7d', '6d', 2], ['Tc', '9d', 0.5], ['Ac', 'Ks', 1]];
  const d = dense(hands);
  const sp = sparse(hands);
  assert.equal(poker.exactHuEquityVsRange(HERO, RIVER, sp), poker.exactHuEquityVsRange(HERO, RIVER, d));
  assert.equal(poker.exactHuEquityVsRange(HERO, TURN, sp), poker.exactHuEquityVsRange(HERO, TURN, d));
  assert.equal(poker.simulateEquityVsRange(HERO, FLOP, sp, 3000, 5), poker.simulateEquityVsRange(HERO, FLOP, d, 3000, 5));
  assert.deepEqual(poker.simulateEquityVsRanges(HERO, FLOP, [sp], 3000, 5), poker.simulateEquityVsRanges(HERO, FLOP, [d], 3000, 5));
  assert.deepEqual(poker.handPotentialBreakdown(HERO, FLOP, sp), poker.handPotentialBreakdown(HERO, FLOP, d));
  assert.deepEqual(
    Array.from(poker.materializeVillainRangeAfterBlockers(sp, HERO, FLOP).weights1326),
    Array.from(poker.materializeVillainRangeAfterBlockers(d, HERO, FLOP).weights1326),
  );
  assert.equal(poker.rangeShannonEntropy(sp), poker.rangeShannonEntropy(d));
});

// Brute force on the legacy evaluator: hero's expected showdown share against weighted opponents,
// over every remaining runout, independently of the package's range code.
function share(heroCards, villains, board) {
  const hs = poker.evaluateBestHand([...heroCards, ...board]).strength;
  let best = hs;
  let tied = 1;
  for (const v of villains) {
    const vs = poker.evaluateBestHand([...v, ...board]).strength;
    if (vs > best) return 0;
    if (vs === best) tied++;
  }
  return best === hs ? 1 / tied : 0;
}
function exactReference(heroCards, board, ranges) {
  let num = 0;
  let den = 0;
  const walk = (k, chosen, w, used) => {
    if (k === ranges.length) {
      const live = DECK.filter((c) => !used.has(c));
      const runouts = board.length === 5 ? [[]] : live.map((c) => [c]);
      for (const r of runouts) {
        num += w * share(heroCards, chosen, [...board, ...r]);
        den += w;
      }
      return;
    }
    for (const [a, b, wt] of ranges[k]) {
      if (used.has(a) || used.has(b)) continue;
      walk(k + 1, [...chosen, [a, b]], w * wt, new Set([...used, a, b]));
    }
  };
  walk(0, [], 1, new Set([...heroCards, ...board]));
  return num / den;
}

test('small weighted ranges match an independent exact enumeration', () => {
  const range = [['Qh', 'Qd', 3], ['7d', '6d', 2], ['Tc', '9d', 0.5], ['Ac', 'Ks', 1], ['9c', '8c', 4]];
  const truthTurn = exactReference(HERO, TURN, [range]);
  near(poker.exactHuEquityVsRange(HERO, TURN, sparse(range)), truthTurn, 1e-12, 'exact HU, turn');
  const mc = poker.simulateEquityVsRanges(HERO, TURN, [sparse(range)], 20000, 9);
  near(mc.estimate, truthTurn, 4.5 * mc.se, 'Monte Carlo HU, turn');
  near(poker.simulateEquityVsRange(HERO, TURN, sparse(range), 20000, 9), truthTurn, 0.02, 'simulateEquityVsRange');

  // Two weighted opponents on the river: the joint law weights every card-disjoint pair by w1·w2.
  const other = [['Qh', 'Qc', 2], ['Kc', 'Kh', 1], ['7d', '6d', 1], ['5h', '5d', 3]];
  const truth2 = exactReference(HERO, RIVER, [range, other]);
  const two = poker.simulateEquityVsRanges(HERO, RIVER, [sparse(range), sparse(other)], 30000, 13);
  near(two.estimate, truth2, 4.5 * two.se, 'two weighted opponents');
});

test('reordering opponents preserves expected equity', () => {
  const a = [['Qh', 'Qd', 3], ['7d', '6d', 2], ['Tc', '9d', 0.5]];
  const b = [['Qh', 'Qc', 2], ['Kc', 'Kh', 1], ['7d', '6c', 1]];
  const c = [['5h', '5d', 1], ['Jh', 'Jc', 2]];
  // The exact joint expectation does not depend on order...
  const ref = exactReference(HERO, RIVER, [a, b, c]);
  near(exactReference(HERO, RIVER, [c, a, b]), ref, 1e-15, 'reference is order-free');
  // ...and each ordering's estimate lands on it within its own error.
  for (const order of [[a, b, c], [c, b, a], [b, c, a]]) {
    const r = poker.simulateEquityVsRanges(HERO, RIVER, order.map((h) => sparse(h)), 20000, 21);
    near(r.estimate, ref, 4.5 * r.se, 'ordering');
  }
  const known = [HERO, ['Qh', 'Qd'], ['7d', '6d'], ['5h', '5d']];
  const e = poker.exactMultiwayEquityKnownHands(known, TURN);
  const swapped = poker.exactMultiwayEquityKnownHands([known[0], known[3], known[1], known[2]], TURN);
  near(swapped[0], e[0], 1e-15, 'exact known hands');
});

test('relabeling suits consistently preserves results', () => {
  const perm = [1, 2, 3, 0]; // c→d, d→h, h→s, s→c
  const relabel = (cards) => poker.applySuitPermToCards(cards, perm);
  const range = scaled(1);
  const moved = poker.applySuitPermToRange1326(range, perm);
  const h2 = relabel(HERO);
  near(poker.exactHuEquityVsRange(h2, relabel(RIVER), moved), poker.exactHuEquityVsRange(HERO, RIVER, range), 1e-15, 'exact river');
  near(poker.exactHuEquityVsRange(h2, relabel(TURN), moved), poker.exactHuEquityVsRange(HERO, TURN, range), 1e-15, 'exact turn');
  const hp = poker.handPotentialBreakdown(HERO, FLOP, range);
  const hp2 = poker.handPotentialBreakdown(h2, relabel(FLOP), moved);
  for (const k of ['hs', 'ppot', 'npot', 'ehs2', 'nAhead']) near(hp2[k], hp[k], 1e-12, `potential ${k}`);
  const r1 = poker.simulateEquityVsRanges(HERO, FLOP, [range], 20000, 4);
  const r2 = poker.simulateEquityVsRanges(h2, relabel(FLOP), [moved], 20000, 4);
  near(r2.estimate, r1.estimate, 4.5 * Math.hypot(r1.se, r2.se), 'Monte Carlo');
});

test('impossible overlapping ranges stop at their attempt budget', () => {
  const aces = sparse([['As', 'Ad', 1]]);
  const start = process.hrtime.bigint();
  assert.throws(
    () => poker.simulateEquityVsRanges(HERO, FLOP, [aces, aces], 500, 1, { maxAttempts: 20000 }),
    /0 of 500 opponent-hand draws were card-disjoint in 20000 attempts/,
  );
  assert.ok(Number(process.hrtime.bigint() - start) / 1e6 < 2000, 'bounded runtime');
  // Rare but possible: each ace combo is disjoint from exactly one of the six, so 1 draw in 6 is kept.
  const allAces = sparse([['As', 'Ad', 1], ['As', 'Ac', 1], ['As', 'Ah', 1], ['Ad', 'Ac', 1], ['Ad', 'Ah', 1], ['Ac', 'Ah', 1]]);
  const ok = poker.simulateEquityVsRanges(['Kc', 'Kd'], FLOP, [allAces, allAces], 3000, 2);
  near(ok.n / ok.attempts, 1 / 6, 0.03, 'acceptance rate');
  assert.throws(
    () => poker.simulateEquityVsRanges(['Kc', 'Kd'], FLOP, [allAces, allAces], 3000, 2, { maxAttempts: 3000 }),
    /card-disjoint in 3000 attempts/,
  );
});
