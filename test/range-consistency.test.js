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

// Fields reported in the caller's weight units. They must scale exactly with the weights; every
// other numeric field (probabilities, frequencies, EVs, entropies, counts, combo and card indices)
// must not change at all.
const WEIGHT_UNIT_FIELDS = {
  handPotentialBreakdown: /^n(Ahead|Tied|Behind)$/,
  materializeVillainRangeAfterBlockers: /^(weights1326\.\d+|weightSum)$/,
  rangeTopCombos: /^\d+\.weight$/,
  rangeBucketWeightsByHandClass: /^(pairs|suitedBroadways|offsuitBroadways|suitedConnectors|suitedAces|other)$/,
  rangeBucketWeightsByNotation: /^\d+\.weight$/,
  rangeBlockerPressureByCard: /^\d+$/,
  blockerMatrixByCard: /^\d+$/,
  applySuitPermToRange1326: /^\d+$/,
  solveHuRiverCheckBetTree: /^topBetCombos\.\d+\.weight$/,
};

test('every weight-unit field pattern names fields that exist', () => {
  for (const [name, pattern] of Object.entries(WEIGHT_UNIT_FIELDS)) {
    assert.ok(CASES[name], `${name} is not a scale case`);
    assert.ok(leaves(CASES[name](1)).some(([k]) => pattern.test(k)), `${name}: ${pattern} matches no field`);
  }
});

// Powers of two scale exactly in binary floating point, and every calculation scales its weights by
// a power of two, so the expectations are exact: probabilities and other scale-free fields are
// bit-identical, and weight-unit fields are multiplied by exactly s.
test('scaling every weight preserves the represented distribution', () => {
  const failures = [];
  for (const [name, call] of Object.entries(CASES)) {
    const base = leaves(call(1));
    assert.ok(base.length > 0, `${name} returned no numbers`);
    const units = WEIGHT_UNIT_FIELDS[name];
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
        const [k2, y] = got[i];
        const expected = units && units.test(k) ? x * s : x;
        if (k2 !== k || !Object.is(y, expected)) {
          failures.push(`${name}.${k} at 2^${Math.log2(s)}: expected ${expected}, got ${y}`);
        }
      });
    }
  }
  assert.deepEqual(failures, []);
});

test('materialized totals agree with their arrays', () => {
  const check = (r, label) => {
    const w = Array.from(r.weights1326);
    assert.equal(r.weightSum, w.reduce((a, b) => a + b, 0), `${label}: weightSum is the sum of weights1326`);
    assert.equal(r.liveComboCount, w.filter((x) => x > 0).length, `${label}: liveComboCount counts positive weights`);
    const m = Math.max(...w);
    const p = w.map((x) => x / m).filter((x) => x > 0); // below 2^-1074 of the maximum adds nothing
    const z = p.reduce((a, b) => a + b, 0);
    const h = -p.reduce((acc, x) => acc + (x / z) * Math.log(x / z), 0);
    near(r.shannonEntropy, h, 1e-12, `${label}: entropy of the normalized array`);
  };
  check(poker.materializeVillainRangeAfterBlockers(scaled(1), HERO, FLOP), 'dense');
  check(poker.materializeVillainRangeAfterBlockers(scaled(2 ** 600), HERO, FLOP), 'dense ×2^600');
  check(poker.materializeVillainRangeAfterBlockers(sparse([['Qh', 'Qd', 3], ['7d', '6d', 2]]), HERO, FLOP), 'sparse');
  // A combo listed twice is one combo carrying the sum of its weights.
  const dup = {
    indices: [id('Qh'), id('Qd'), id('7d'), id('6d'), id('Qd'), id('Qh')],
    weights: [2, 1, 3],
  };
  const r = poker.materializeVillainRangeAfterBlockers(dup, HERO, FLOP);
  check(r, 'duplicates');
  assert.equal(r.weights1326[comboIndex(id('Qh'), id('Qd'))], 5);
  assert.equal(r.liveComboCount, 2);
  assert.equal(r.weightSum, 6);
  // Weights 1e200 and 1e-200 are both representable and both survive.
  const wide = poker.materializeVillainRangeAfterBlockers(sparse([['Qh', 'Qd', 1e200], ['7d', '6d', 1e-200]]), HERO, FLOP);
  check(wide, 'wide');
  assert.equal(wide.weights1326[comboIndex(id('Qh'), id('Qd'))], 1e200);
  assert.equal(wide.weights1326[comboIndex(id('7d'), id('6d'))], 1e-200);
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

// A scale must come from the combos that take part in a calculation. Each range below adds one combo
// that hero's Ah blocks, weighted 2^2000 times the others: scaling by it would zero every combo that
// is left. The results must equal those of the live combos alone, bit for bit (2^-1000 is exact).
test('a dominant combo that drops out does not erase the combos that remain', () => {
  const live = [['Qh', 'Qd', 3], ['7d', '6d', 2], ['Tc', '9d', 0.5]];
  const withBlocked = (scale) => sparse([...live.map(([a, b, w]) => [a, b, w * scale]), ['Ah', 'As', 2 ** 1000]]);
  const plain = sparse(live);
  const tiny = withBlocked(2 ** -1000);
  assert.equal(poker.exactHuEquityVsRange(HERO, RIVER, tiny), poker.exactHuEquityVsRange(HERO, RIVER, plain));
  assert.equal(poker.exactHuEquityVsRange(HERO, TURN, tiny), poker.exactHuEquityVsRange(HERO, TURN, plain));
  assert.equal(poker.exactRangeDominatedComboFraction(HERO, RIVER, tiny), poker.exactRangeDominatedComboFraction(HERO, RIVER, plain));
  assert.equal(poker.equityDeltaIfCardRemoved(HERO, TURN, tiny, id('9c')), poker.equityDeltaIfCardRemoved(HERO, TURN, plain, id('9c')));
  assert.equal(poker.handStrengthVsRange(HERO, FLOP, tiny), poker.handStrengthVsRange(HERO, FLOP, plain));
  const hp = poker.handPotentialBreakdown(HERO, FLOP, plain);
  const hpTiny = poker.handPotentialBreakdown(HERO, FLOP, tiny);
  for (const k of ['hs', 'ppot', 'npot', 'ehs', 'ehs2']) assert.equal(hpTiny[k], hp[k], `potential ${k}`);
  for (const k of ['nAhead', 'nTied', 'nBehind']) assert.equal(hpTiny[k], hp[k] * 2 ** -1000, `${k} in caller units`);
  // Monte Carlo draws only from live combos, so the same seed picks the same hands.
  assert.equal(poker.simulateEquityVsRange(HERO, FLOP, tiny, 3000, 5), poker.simulateEquityVsRange(HERO, FLOP, plain, 3000, 5));
  assert.deepEqual(
    poker.simulateEquityVsRanges(HERO, FLOP, [tiny, withBlocked(2 ** -900)], 3000, 5),
    poker.simulateEquityVsRanges(HERO, FLOP, [plain, plain], 3000, 5),
  );
  const m = poker.materializeVillainRangeAfterBlockers(tiny, HERO, FLOP);
  assert.deepEqual(Array.from(m.weights1326), Array.from(poker.materializeVillainRangeAfterBlockers(plain, HERO, FLOP).weights1326, (w) => w * 2 ** -1000));
  assert.equal(m.liveComboCount, 3);
  // A combo outside every bucket (blocked by the board) cannot set the scale for the rest.
  const boardBlocked = sparse([...live.map(([a, b, w]) => [a, b, w * 2 ** -1000]), ['Qs', 'Kc', 2 ** 1000]]);
  assert.deepEqual(poker.bucketMassFromRange(boardBlocked, BUCKETS(), 20), poker.bucketMassFromRange(plain, BUCKETS(), 20));
  // Omaha: the 2^1000 combo holds hero's Ah.
  const omaha = (w) => ({ packed: Uint8Array.of(4, 9, 14, 19, 6, 11, 16, 21, id('Ah'), 8, 13, 18), weights: [w, 2 * w, 2 ** 1000] });
  const omahaHero = ['Ah', 'Kd', 'Jh', 'Tc'];
  assert.equal(
    poker.simulateOmahaEquityVsRange(omahaHero, FLOP, omaha(2 ** -1000), 400, 3),
    poker.simulateOmahaEquityVsRange(omahaHero, FLOP, OMAHA(1), 400, 3),
  );
  // Short deck: every AA combo is dead (hero AhAd, board AsAc).
  const sdHero = ['Ah', 'Ad'];
  const sdBoard = ['As', 'Ac', 'Kd'];
  assert.equal(
    poker.simulateShortDeckEquityVsRange(sdHero, sdBoard, { AA: 2 ** 1000, KQs: 2 ** -1000, 77: 3 * 2 ** -1000 }, 400, 3),
    poker.simulateShortDeckEquityVsRange(sdHero, sdBoard, { KQs: 1, 77: 3 }, 400, 3),
  );
});

// The reported CFR case: each range's 1e200 combo (KhKd) conflicts with the other's, so only the
// two cross pairs carry weight (1e200 × 1e-200 = 1 each) and bettor EV is (112.5 + 12.5) / 2.
test('river CFR weighs card-compatible pairs, not each range on its own scale', () => {
  const board = ['2c', '7d', '9s', 'Jc', 'Qh'];
  const bettor = { indices: [id('Kh'), id('Kd'), id('3c'), id('4d')], weights: [1e200, 1e-200] };
  const defender = { indices: [id('Kh'), id('Kd'), id('5c'), id('6d')], weights: [1e200, 1e-200] };
  assert.deepEqual(poker.evOfStrategyProfile(100, 50, bettor, defender, board, 0.5, 0.5), { evBettor: 62.5, evDefender: 37.5 });
  const pow = (r, s) => ({ indices: r.indices, weights: r.weights.map((w) => w * s) });
  for (const s of [2 ** 100, 2 ** -100]) {
    assert.deepEqual(poker.evOfStrategyProfile(100, 50, pow(bettor, s), pow(defender, s), board, 0.5, 0.5), { evBettor: 62.5, evDefender: 37.5 });
  }
  // The same holds for every solver on these ranges: they agree with the equal-weight pairs.
  const ev = poker.exploitabilityRiver(100, 50, bettor, defender, board, 0.5, 0.5);
  assert.ok(Number.isFinite(ev) && ev >= 0, `exploitability ${ev}`);
  // A class average uses its own members: the air class is 3c4d alone (1e-200), so its bet rate is
  // 3c4d's own, even though the range's largest weight is 1e200.
  const tree = poker.solveHuRiverCheckBetTree(100, 50, bettor, defender, board, 40);
  const air = tree.topBetCombos.find((c) => c.comboIndex === comboIndex(id('3c'), id('4d')));
  assert.equal(air.weight, 1e-200);
  assert.ok(air.betFrequency > 0);
  near(tree.classes.airBet, air.betFrequency, 1e-15, 'air class bet rate');
  assert.equal(tree.betMix[air.comboIndex], air.betFrequency);
  // No pair can be dealt: rejected rather than reported as EV 0.
  const kings = { indices: [id('Kh'), id('Kd')], weights: [1] };
  assert.throws(() => poker.evOfStrategyProfile(100, 50, kings, kings, board, 0.5, 0.5), /shares a card with every/);
});

// One call per native range parser (each binding file), taking the range as `r`.
const PARSER_CASES = {
  exactHuEquityVsRange: (r) => poker.exactHuEquityVsRange(HERO, RIVER, r),
  simulateEquityVsRanges: (r) => poker.simulateEquityVsRanges(HERO, FLOP, [r], 500, 3),
  exactRangeDominatedComboFraction: (r) => poker.exactRangeDominatedComboFraction(HERO, RIVER, r),
  exactEquityCardRemovalGradient: (r) => poker.exactEquityCardRemovalGradient(HERO, TURN, r),
  exactInformationRegretVsClairvoyant: (r) => poker.exactInformationRegretVsClairvoyant(HERO, TURN, r, 100, 20),
  materializeVillainRangeAfterBlockers: (r) => poker.materializeVillainRangeAfterBlockers(r, HERO, FLOP),
  handPotentialBreakdown: (r) => poker.handPotentialBreakdown(HERO, FLOP, r),
  bucketMassFromRange: (r) => poker.bucketMassFromRange(r, BUCKETS(), 20),
  evOfStrategyProfile: (r) => poker.evOfStrategyProfile(100, 50, r, r, RIVER, 0.5, 0.5),
  normalizeSparseRange: (r) => poker.normalizeSparseRange(r),
  rangeShannonEntropy: (r) => poker.rangeShannonEntropy(r),
};
const PARSER_HANDS = [['Qh', 'Qd', 3], ['7d', '6d', 2], ['Tc', '9d', 0.5], ['Ac', 'Ks', 1]];

test('number[], Int32Array, and Uint32Array indices give identical results everywhere', () => {
  const base = sparse(PARSER_HANDS);
  const plain = Array.from(base.indices);
  for (const [name, call] of Object.entries(PARSER_CASES)) {
    const expected = call(base);
    assert.deepEqual(call({ indices: plain, weights: Array.from(base.weights) }), expected, `${name}: number[]`);
    assert.deepEqual(call({ indices: Uint32Array.from(plain), weights: base.weights }), expected, `${name}: Uint32Array`);
    assert.deepEqual(call(dense(PARSER_HANDS)), expected, `${name}: dense`);
  }
});

test('a combo listed more than once is one combo carrying the summed weight', () => {
  // Same combos and order as `sparse(PARSER_HANDS)`, with QhQd split over two entries (reversed).
  const split = sparse(PARSER_HANDS);
  const indices = [...split.indices, id('Qd'), id('Qh')];
  const weights = [...split.weights];
  const qqIndex = comboIndex(id('Qh'), id('Qd'));
  const qq = weights.findIndex((_, i) => comboIndex(indices[2 * i], indices[2 * i + 1]) === qqIndex);
  assert.ok(qq >= 0);
  weights[qq] = 1;
  weights.push(2);
  for (const [name, call] of Object.entries(PARSER_CASES)) {
    assert.deepEqual(call({ indices, weights }), call(sparse(PARSER_HANDS)), name);
  }
  assert.throws(
    () => poker.exactHuEquityVsRange(HERO, RIVER, { indices: [id('Qh'), id('Qd'), id('Qd'), id('Qh')], weights: [1.5e308, 1.5e308] }),
    /listed more than once/,
  );
});

test('malformed deck ids are rejected before they can become another card', () => {
  for (const bad of [NaN, 1.9, -1, 52, 2 ** 31, 4294967297, Infinity, '3', null]) {
    for (const [name, call] of Object.entries(PARSER_CASES)) {
      assert.throws(() => call({ indices: [bad, id('Qd')], weights: [1] }), /indices\[0\] must be an integer deck id 0\.\.51/, `${name}: ${bad}`);
    }
  }
  assert.throws(() => poker.exactHuEquityVsRange(HERO, RIVER, { indices: Int32Array.of(-1, 5), weights: [1] }), /indices\[0\]/);
  assert.throws(() => poker.exactHuEquityVsRange(HERO, RIVER, { indices: Uint32Array.of(5, 52), weights: [1] }), /indices\[1\]/);
  assert.throws(() => poker.exactHuEquityVsRange(HERO, RIVER, { indices: Float64Array.of(5, 9), weights: [1] }), /number\[\], Int32Array, or Uint32Array/);
  // Packed Omaha and Big O combos: checked as numbers, and a combo cannot repeat a card.
  const omahaHero = ['Ah', 'Kd', 'Jh', 'Tc'];
  for (const bad of [NaN, 1.5, -1, 52, 4294967300]) {
    assert.throws(() => poker.simulateOmahaEquityVsRange(omahaHero, FLOP, { packed: [bad, 9, 14, 19] }, 10, 1), /integer 0\.\.51/, `omaha ${bad}`);
    assert.throws(() => poker.simulateBigOEquityVsRange([...omahaHero, '9s'], FLOP, { packed: [4, 9, 14, 19, bad] }, 10, 1), /integer 0\.\.51/, `big O ${bad}`);
  }
  assert.throws(() => poker.simulateOmahaEquityVsRange(omahaHero, FLOP, { packed: [4, 9, 9, 19] }, 10, 1), /repeats a card/);
});

test('the attempt budget is honoured or the call is rejected', () => {
  const one = sparse([['Qh', 'Qd', 1]]);
  assert.throws(() => poker.simulateEquityVsRanges(HERO, FLOP, [one], 100, 1, { maxAttempts: 1 }), /at least numSimulations/);
  assert.throws(() => poker.simulateEquityVsRanges(HERO, FLOP, [one], 100, 1, { maxAttempts: 99 }), /at least numSimulations/);
  // One range never collides with itself, so exactly one attempt per sample is enough.
  const r = poker.simulateEquityVsRanges(HERO, FLOP, [one], 100, 1, { maxAttempts: 100 });
  assert.equal(r.n, 100);
  assert.equal(r.attempts, 100);
});
