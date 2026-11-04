'use strict';

// Regression checks for the audited 4.1.2 defects. Expected values come from independent references
// (closed forms, brute-force enumeration, a small reference CFR, the Wilson formula), not from the
// package's own output.

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const { Worker } = require('node:worker_threads');

const root = path.join(__dirname, '..');
const poker = require(root);
const encode = require(path.join(root, 'encode'));

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';
const DECK = [];
for (const r of RANKS) for (const s of SUITS) DECK.push(r + s);
const id = (c) => RANKS.indexOf(c[0]) * 4 + SUITS.indexOf(c[1]);
const sparse = (hands) => ({ indices: hands.flatMap(([a, b]) => [id(a), id(b)]), weights: hands.map(([, , w]) => w) });

function near(actual, expected, tol, label) {
  assert.ok(Math.abs(actual - expected) <= tol, `${label}: expected ${expected}, got ${actual} (tol ${tol})`);
}
const timed = (fn) => {
  const t0 = process.hrtime.bigint();
  const r = fn();
  return [r, Number(process.hrtime.bigint() - t0) / 1e6];
};

test('derived formulas invert their forward helpers', () => {
  // Outs: the rules of two and four are 2% and 4% per out.
  assert.equal(poker.estimatedOutsFromRuleOfTwo(poker.ruleOfTwoEquity(9), 47), 9);
  assert.equal(poker.estimatedOutsFromRuleOfFour(poker.ruleOfFourEquity(9), 47), 9);
  assert.equal(poker.estimatedOutsFromRuleOfTwo(1, 47), 47, 'capped at the unseen cards');
  assert.throws(() => poker.estimatedOutsFromRuleOfTwo(1.2, 47), /\[0, 1\]/);
  // Reverse implied odds: e*P - (1-e)(C+L) = 0.
  for (const [p, c, l] of [[100, 50, 50], [100, 50, 0], [30, 10, 90]]) {
    const e = poker.reverseImpliedOddsMinEquity(p, c, l);
    near(e, (c + l) / (p + c + l), 1e-15, `reverse implied ${p},${c},${l}`);
    near(e * p - (1 - e) * (c + l), 0, 1e-12, 'EV at the threshold');
    assert.ok(e >= poker.breakevenCallEquity(p, c) - 1e-15, 'extra losses never lower the threshold');
  }
  // Bet sizing: EV(B) = fe*P + (1-fe)(eq(P+2B) - B).
  const evAt = (pot, fe, eq, b) => fe * pot + (1 - fe) * (eq * (pot + 2 * b) - b);
  const b = poker.betSizingIndifferencePoint(100, 0.2, 0.3);
  near(b, 137.5, 1e-9, 'finite root');
  near(evAt(100, 0.2, 0.3, b), 0, 1e-9, 'EV at the root');
  assert.equal(poker.betSizingIndifferencePoint(100, 0, 0.5), Infinity, 'EV is +50 at every size');
  assert.equal(poker.betSizingIndifferencePoint(100, 1, 0.1), Infinity, 'always folds');
  assert.equal(poker.betSizingIndifferencePoint(100, 0.3, 0.8), Infinity, 'EV grows with size');
  assert.equal(poker.betSizingIndifferencePoint(0, 0, 0.2), 0);
  assert.throws(() => poker.betSizingIndifferencePoint(100, 1.5, 0.3), /foldFrequency/);
});

// Wilson half-width straight from the formula, for the planner check.
function wilsonHalf(p, n, z) {
  return (z * Math.sqrt((p * (1 - p)) / n + (z * z) / (4 * n * n))) / (1 + (z * z) / n);
}

test('trial planners meet their bound with the smallest count, and refuse unrepresentable ones', () => {
  for (const [p, h] of [[0.001, 0.01], [0.01, 0.01], [0.05, 0.01], [0.5, 0.001], [0.3, 0.2]]) {
    const n = poker.monteCarloTrialsForWilsonHalfWidth(p, h, 1.96);
    assert.ok(wilsonHalf(p, n, 1.96) <= h, `Wilson ${p}: half-width at n=${n} meets ${h}`);
    if (n > 1) assert.ok(wilsonHalf(p, n - 1, 1.96) > h, `Wilson ${p}: n=${n} is the smallest`);
  }
  for (const [p, se] of [[0.5, 0.01], [0.1, 0.003]]) {
    const n = poker.monteCarloTrialsForStandardErrorBound(p, se);
    assert.ok(Math.sqrt((p * (1 - p)) / n) <= se && (n === 1 || Math.sqrt((p * (1 - p)) / (n - 1)) > se));
  }
  assert.throws(() => poker.monteCarloTrialsForStandardErrorBound(0.5, 1e-200), /2\^53 - 1/);
  assert.throws(() => poker.monteCarloTrialsForHoeffdingBound(1e-10, 0.05), /2\^53 - 1/);
  assert.throws(() => poker.monteCarloTrialsForWilsonHalfWidth(0.5, 1e-200, 1.96), /2\^53 - 1/);
  assert.equal(poker.monteCarloTrialsForHoeffdingBound(0.01, 0.05), Math.ceil(Math.log(2 / 0.05) / (2 * 0.01 * 0.01)));
});

// Gambler's ruin by solving the absorbing chain directly (small h + v).
function ruinReference(h, v, p) {
  const n = h + v;
  const solve = (rhs) => {
    // x_0 = rhs0, x_n = rhsN; x_k = p x_{k+1} + q x_{k-1} + c (Thomas algorithm on k = 1..n-1).
    const q = 1 - p;
    const a = [], bb = [], c = [], d = [];
    for (let k = 1; k < n; k++) { a.push(-q); bb.push(1); c.push(-p); d.push(rhs.c); }
    d[0] += q * rhs.x0;
    d[n - 2] += p * rhs.xn;
    for (let i = 1; i < n - 1; i++) {
      const m = a[i] / bb[i - 1];
      bb[i] -= m * c[i - 1];
      d[i] -= m * d[i - 1];
    }
    const x = new Array(n - 1);
    x[n - 2] = d[n - 2] / bb[n - 2];
    for (let i = n - 3; i >= 0; i--) x[i] = (d[i] - c[i] * x[i + 1]) / bb[i];
    return x[h - 1];
  };
  return { win: solve({ c: 0, x0: 0, xn: 1 }), hands: solve({ c: 1, x0: 0, xn: 0 }) };
}

test('tournament duel absorption is finite and matches the absorbing chain', () => {
  const r = poker.tournamentDuelAbsorptionProbabilities(1000, 1000, 0.1, 1, 100);
  assert.ok(Number.isFinite(r.heroWinProbability) && r.heroWinProbability >= 0 && r.heroWinProbability < 1e-300);
  near(r.expectedHands, 1250, 1e-9, 'expected hands h / (q - p)');
  assert.equal(r.heroPrizeEv, 100 * r.heroWinProbability);
  for (const [h, v, p] of [[3, 7, 0.3], [10, 30, 0.5000001], [10, 30, 0.5], [25, 5, 0.55], [12, 12, 0.49]]) {
    const ref = ruinReference(h, v, p);
    const got = poker.tournamentDuelAbsorptionProbabilities(h, v, p, 1, 1);
    near(got.heroWinProbability, ref.win, 1e-11, `win ${h},${v},${p}`);
    near(got.expectedHands, ref.hands, 1e-9 * ref.hands, `hands ${h},${v},${p}`);
  }
  assert.throws(() => poker.tournamentDuelAbsorptionProbabilities(NaN, 10, 0.5, 1, 1), /heroStack/);
});

test('normalization outside ranges keeps proportions at extreme scales', () => {
  assert.deepEqual(Array.from(poker.normalizedStackFractions([1e308, 1e308])), [0.5, 0.5]);
  assert.deepEqual(Array.from(poker.icmExpectedPayouts([1e308, 1e308], [100, 100])), [100, 100]);
  // Conditioning over the players still unplaced keeps the small seats apart.
  assert.deepEqual(Array.from(poker.icmExpectedPayouts([1e308, 1e-308, 1e-308], [100, 50, 0])), [100, 25, 25]);
  assert.equal(poker.betaBinomialFoldPosterior(1e308, 1e308, 0, 0).posteriorMean, 0.5);
  assert.deepEqual(Array.from(poker.regretMatchingStrategy([1e308, 1e308])), [0.5, 0.5]);
  assert.equal(poker.mysteryBountyExpectedValue([100, 1, 1], [1e308, 1e308, 1e308], 1).oneDraw, 34);
  assert.deepEqual(Array.from(poker.icmExpectedPayoutsWeitzman([1e308, 1e308], [100, 0], 2)), [50, 50]);
  const base = Array.from(poker.icmExpectedPayouts([3, 2, 1, 1], [50, 30, 20, 0]));
  for (const k of [2 ** 600, 2 ** -600]) {
    assert.deepEqual(Array.from(poker.icmExpectedPayouts([3, 2, 1, 1].map((s) => s * k), [50, 30, 20, 0])), base, `ICM scale ${k}`);
  }
});

// Harville by brute force over finishing orders (small fields).
function harvilleBrute(weights) {
  const n = weights.length;
  const place = weights.map(() => new Array(n).fill(0));
  const walk = (left, j, prob) => {
    if (left.length === 0) return;
    const sum = left.reduce((a, i) => a + weights[i], 0);
    for (const i of left) {
      const pr = sum > 0 ? weights[i] / sum : 1 / left.length;
      place[i][j] += prob * pr;
      walk(left.filter((x) => x !== i), j + 1, prob * pr);
    }
  };
  walk([...weights.keys()], 0, 1);
  return place;
}

test('ICM matches brute-force Harville, conserves the pool, and avoids factorial work', () => {
  const stacks = [5, 3, 2, 1, 1];
  const pay = [50, 25, 15, 10, 0];
  const brute = harvilleBrute(stacks);
  const flat = Array.from(poker.icmHarvillePlacementProbabilities(stacks));
  brute.forEach((row, i) => row.forEach((x, j) => near(flat[i * 5 + j], x, 1e-14, `placement ${i},${j}`)));
  const ev = Array.from(poker.icmExpectedPayouts(stacks, pay));
  ev.forEach((x, i) => near(x, brute[i].reduce((a, p, j) => a + p * pay[j], 0), 1e-12, `payout ${i}`));
  near(ev.reduce((a, b) => a + b, 0), 100, 1e-12, 'pool conserved');
  // Twelve equal stacks: first place is the chip share, and equal prizes need no enumeration.
  const [win, t1] = timed(() => poker.icmWinProbabilitiesHarville(Array(12).fill(1)));
  Array.from(win).forEach((x) => near(x, 1 / 12, 1e-15, 'win share'));
  const [pays, t2] = timed(() => poker.icmExpectedPayouts(Array(12).fill(1), Array(12).fill(100)));
  assert.deepEqual(Array.from(pays), Array(12).fill(100));
  assert.ok(t1 < 100 && t2 < 100, `fast: ${t1} ms, ${t2} ms`);
  // A few paid places in a big field stay cheap; a full 25-player matrix is outside the budget.
  const big = Array.from({ length: 31 }, (_, i) => i + 1);
  near(Array.from(poker.icmExpectedPayouts(big, [50, 30, 20, ...Array(28).fill(0)])).reduce((a, b) => a + b, 0), 100, 1e-9, 'big field');
  assert.throws(() => poker.icmHarvillePlacementProbabilities(Array(25).fill(1)), /subset states/);
  // Skill-adjusted: zero first-place weights finish last, in a uniformly random order.
  assert.deepEqual(Array.from(poker.icmHarvilleSkillAdjustedPayouts([1, 1, 1], [60, 40, 0], [0, 0, 1], 1)), [20, 20, 60]);
});

test('ICM stack Jacobian is the analytical derivative', () => {
  const j = Array.from(poker.icmHarvilleStackJacobian([1, 1], [100, 0]));
  [25, -25, -25, 25].forEach((x, i) => near(j[i], x, 1e-6, `J ${i}`));
  const scaled = Array.from(poker.icmHarvilleStackJacobian([1000, 1000], [100, 0]));
  scaled.forEach((x, i) => near(x, j[i] / 1000, 1e-9, 'scales as 1 / stack'));
  const s = [4, 3, 2, 1];
  const jj = Array.from(poker.icmHarvilleStackJacobian(s, [50, 30, 20, 0]));
  for (let c = 0; c < 4; c++) near(jj.filter((_, k) => k % 4 === c).reduce((a, b) => a + b, 0), 0, 1e-6, `column ${c} sums to 0`);
  for (let r = 0; r < 4; r++) near(s.reduce((a, x, c) => a + x * jj[r * 4 + c], 0), 0, 1e-6, `Euler row ${r}`);
});

test('side-pot ladders conserve chips and only count real contributors', () => {
  const total = (layers) => layers.reduce((a, l) => a + l.potChips, 0);
  const sum = (xs) => xs.reduce((a, b) => a + b, 0);
  for (const c of [[1e-13, 2e-13], [0, 1e-13], [10, 1, 100], [0.3, 0.1, 0.2, 0.2], [5, 5, 5], [7, 0, 0, 3]]) {
    const layers = poker.sidePotLadderFromCommitments(c);
    near(total(layers), sum(c), 4 * Number.EPSILON * sum(c), `conserved ${c}`);
    for (const l of layers) {
      Array.from(l.playerCapContribution).forEach((x, i) => assert.ok(x === 0 || c[i] > 0, `player ${i} with nothing in contributes`));
    }
  }
  const zero = poker.sidePotLadderFromCommitments([0, 1e-13]);
  assert.deepEqual(Array.from(zero[0].playerCapContribution), [0, 1e-13]);
  // Powers of two scale exactly.
  const a = poker.sidePotLadderFromCommitments([3, 1, 2]).map((l) => l.potChips);
  assert.deepEqual(poker.sidePotLadderFromCommitments([3, 1, 2].map((x) => x * 2 ** 40)).map((l) => l.potChips), a.map((x) => x * 2 ** 40));
});

test('legal actions do not overflow on large accepted states', () => {
  const state = {
    players: [
      { holeCards: ['Ah', 'Ad'], stack: 800000000, seat: 0 },
      { holeCards: ['Kh', 'Kd'], stack: 0, committedThisStreet: 1200000000, seat: 1 },
    ],
    phase: 'river', communityCards: ['2c', '7d', '9s', 'Jc', 'Qh'],
    pot: 1200000000, currentBet: 1200000000, lastRaiseIncrement: 1200000000,
    actedThisStreet: [false, true], actingIndex: 0,
  };
  assert.equal(poker.validatePokerState(state).valid, true);
  const legal = poker.legalActionSummary(state);
  assert.equal(legal.minRaiseTo, 800000000, 'all-in when shorter than a full raise');
  assert.equal(legal.maxRaiseTo, 800000000);
  assert.equal(legal.canRaise, false, 'nobody can answer a raise');
  assert.equal(legal.canCall, true);
});

// Vanilla CFR with opponent reach on the analytic river game (independent of the package).
// Bettor: value hand (always wins vs the bluff catcher) and bluff (always loses); defender: one hand.
function referenceCfr(iters, pot, bet) {
  const rb = [[0, 0], [0, 0]]; // [check, bet] regrets for value, bluff
  const rc = [0, 0]; // [fold, call]
  const sumBet = [0, 0];
  let sumCall = 0;
  const rm = (r) => {
    const a = Math.max(0, r[0]), b = Math.max(0, r[1]);
    return a + b > 0 ? b / (a + b) : 0.5;
  };
  for (let t = 0; t < iters; t++) {
    const betP = [rm(rb[0]), rm(rb[1])];
    const callP = rm(rc);
    sumBet[0] += betP[0]; sumBet[1] += betP[1]; sumCall += callP;
    // Bettor values (chance weight 1/2 each, constant per hand).
    const eqs = [1, 0];
    for (let h = 0; h < 2; h++) {
      const check = eqs[h] * pot;
      const betv = (1 - callP) * pot + callP * (eqs[h] * (pot + 2 * bet) - bet);
      const sigma = (1 - betP[h]) * check + betP[h] * betv;
      rb[h][0] += check - sigma; rb[h][1] += betv - sigma;
    }
    // Defender counterfactual value of calling: reach-weighted by each bettor hand's bet frequency.
    let callCf = 0;
    for (let h = 0; h < 2; h++) callCf += 0.5 * betP[h] * ((1 - eqs[h]) * (pot + 2 * bet) - bet);
    const sigma = callP * callCf;
    rc[0] += 0 - sigma; rc[1] += callCf - sigma;
  }
  return { bet: [sumBet[0] / iters, sumBet[1] / iters], call: sumCall / iters };
}

test('river CFR keeps opponent reach in its regrets and converges to the analytic equilibrium', () => {
  const board = ['2c', '7d', '9s', 'Jc', 'Qh'];
  const bettor = sparse([['Qc', 'Qd', 1], ['3c', '4d', 1]]);
  const defender = sparse([['Ah', 'Ad', 1]]);
  const ref = referenceCfr(20000, 100, 50);
  for (const solve of [poker.cfrRiverBetCallFoldSolve, poker.solveHuRiverCheckBetTree]) {
    const r = solve(100, 50, bettor, defender, board, 20000);
    near(r.betFreq, (ref.bet[0] + ref.bet[1]) / 2, 1e-9, 'matches the reference CFR');
    near(r.callFreq, ref.call, 1e-9, 'defender matches the reference CFR');
    near(r.betFreq, 2 / 3, 0.01, 'aggregate bet near 2/3');
    near(r.evBettor, 200 / 3, 0.01, 'bettor EV near 66.67');
    assert.ok(poker.exploitabilityRiver(100, 50, bettor, defender, board, r.betMix, r.callMix) < 0.1);
  }
  // Fictitious play is unchanged and also converges.
  const fp = poker.fictitiousPlayRiver(100, 50, bettor, defender, board, 20000);
  near(fp.betFreq, 2 / 3, 0.01, 'fictitious play');
});

test('Nash push/fold root values conserve chips and prizes over one joint deal', () => {
  const ich = poker.nashIcmHeadsUpJamCallSolve({
    heroStack: 10, villainStack: 10, smallBlind: 0.5, bigBlind: 1, payouts: [100, 0],
    equityIterations: 1, equitySeed: 42, maxIterations: 1,
  });
  near(ich.heroEv + ich.villainEv, 100, 1e-9, 'heads-up ICM pool');
  for (const spec of [
    { heroStack: 10, villainStack: 10, smallBlind: 0.5, bigBlind: 1 },
    { heroStack: 10, villainStack: 30, smallBlind: 0.5, bigBlind: 1, ante: 0.4 },
    { heroStack: 25, villainStack: 8, smallBlind: 0.5, bigBlind: 1, ante: 1 },
  ]) {
    const r = poker.nashHeadsUpJamCallSolve({ ...spec, equityIterations: 20, maxIterations: 40 });
    near(r.heroEv + r.villainEv, spec.ante || 0, 1e-9, `chip EVs sum to the dead ante: ${JSON.stringify(spec)}`);
  }
  const icm2 = poker.nashIcmHeadsUpJamCallSolve({
    heroStack: 10, villainStack: 30, smallBlind: 0.5, bigBlind: 1, ante: 0.4, payouts: [70, 30],
    equityIterations: 5, equitySeed: 3, maxIterations: 20,
  });
  near(icm2.heroEv + icm2.villainEv, 100, 1e-9, 'unequal stacks with antes');
});

function runWorkers(jobs) {
  const code = `
    const { parentPort, workerData } = require('node:worker_threads');
    const poker = require(workerData.root);
    const out = [];
    for (const job of workerData.jobs) {
      if (job.kind === 'holdem') {
        out.push(Array.from(poker.nashHeadsUpJamRange({ stackBb: 10, equityIterations: 1, equitySeed: job.seed, maxIterations: 100 })));
      } else {
        out.push(Array.from(poker.shortDeckNashHuJamRange({ stackBb: 10, equityIterations: 1, equitySeed: job.seed, maxIterations: 100 })));
      }
    }
    parentPort.postMessage(out);
  `;
  return new Promise((resolve, reject) => {
    const w = new Worker(code, { eval: true, workerData: { root, jobs } });
    w.once('message', resolve);
    w.once('error', reject);
  });
}

test('concurrent Nash solves agree with serial ones (both equity caches)', async () => {
  for (const kind of ['holdem', 'shortdeck']) {
    const steady = Array.from({ length: 30 }, () => ({ kind, seed: 42 }));
    const varied = Array.from({ length: 30 }, (_, i) => ({ kind, seed: 100 + i }));
    const solve = (job) => Array.from(job.kind === 'holdem'
      ? poker.nashHeadsUpJamRange({ stackBb: 10, equityIterations: 1, equitySeed: job.seed, maxIterations: 100 })
      : poker.shortDeckNashHuJamRange({ stackBb: 10, equityIterations: 1, equitySeed: job.seed, maxIterations: 100 }));
    const serialSteady = solve(steady[0]);
    const serialVaried = varied.map(solve);
    const [a, b] = await Promise.all([runWorkers(steady), runWorkers(varied)]);
    a.forEach((r, i) => assert.deepEqual(r, serialSteady, `${kind} seed 42, run ${i}`));
    b.forEach((r, i) => assert.deepEqual(r, serialVaried[i], `${kind} seed ${100 + i}`));
  }
});

test('PKO EVs use physical fold terminals', () => {
  // Folding hands the 100-chip pot to the shover: stacks [100, 200], winner-take-all ICM 100/3.
  const call = poker.pkoCallEvVsShove([100, 100], [100, 0], [0, 0], 0, 1, 100, 0);
  near(call.foldEv, 100 / 3, 1e-12, 'fold EV');
  const jam = poker.pkoJamEvVsFold([100, 100], [100, 0], [0, 0], 0, 1, 100, 1, 0.5);
  near(jam.foldEv, 100 / 3, 1e-12, 'hero folding gives the pot away');
  near(jam.jamEv, 200 / 3, 1e-12, 'villain always folds: hero takes the pot');
  // Check-down: the pot goes to the showdown winner with no further chips.
  const hunt = poker.pkoCoveringHuntEv([200, 100], [100, 0], [0, 0], 0, 1, 50, 0.5);
  near(hunt.checkDownEv, 0.5 * (250 / 350) * 100 + 0.5 * (200 / 350) * 100, 1e-9, 'check-down EV');
  // Documented contracts: the deprecated helpers keep their stated formulas.
  assert.deepEqual(Array.from(poker.icmExpectedPayoutsWeitzman([1, 3], [100, 100], 2)).map((x) => Math.round(x)), [20, 180]);
  near(poker.stackToPotAfterCall(100, 50, 500), 1 / poker.sprAfterCall(100, 50, 500), 1e-15, 'inverse SPR');
});

// Expected total of k weighted draws without replacement, by enumerating draw orders.
function mysteryBrute(values, weights, k) {
  let total = 0;
  const walk = (left, depth, prob, acc) => {
    if (depth === k) { total += prob * acc; return; }
    const sum = left.reduce((a, i) => a + weights[i], 0);
    for (const i of left) walk(left.filter((x) => x !== i), depth + 1, prob * (weights[i] / sum), acc + values[i]);
  };
  walk([...values.keys()], 0, 1, 0);
  return total;
}

test('mystery bounty draws follow the finite pool', () => {
  const values = [100, 1, 1];
  const weights = [1000, 1, 1];
  const ks = [0, 1, 2, 3].map((k) => poker.mysteryBountyExpectedValue(values, weights, k).sampleK);
  ks.forEach((x, k) => near(x, mysteryBrute(values, weights, k), 1e-9, `k = ${k}`));
  assert.ok(ks.every((x, i) => i === 0 || x >= ks[i - 1]), 'monotone in k');
  assert.ok(ks.every((x) => x <= 102 + 1e-12), 'never above the pool');
  const v4 = [40, 30, 20, 10];
  const w4 = [1, 2, 3, 4];
  for (const k of [1, 2, 3, 4]) near(poker.mysteryBountyExpectedValue(v4, w4, k).sampleK, mysteryBrute(v4, w4, k), 1e-9, `4-pool k ${k}`);
  near(poker.mysteryBountyExpectedValue([10, 20, 30], undefined, 2).sampleK, 40, 1e-12, 'equal weights');
});

test('blocker pressure honors dead cards', () => {
  const range = { indices: [id('Ah'), id('Ad'), id('Kh'), id('Kd')], weights: [1, 1] };
  const r = poker.rangeBlockerPressureByCard(range, ['Ah']);
  assert.deepEqual([r[id('Ah')], r[id('Ad')], r[id('Kh')], r[id('Kd')]], [0, 0, 1, 1]);
  const m = poker.blockerMatrixByCard(range, ['Kd']);
  assert.equal(m.length, 52);
  assert.deepEqual([m[id('Ah')], m[id('Kh')]], [1, 0]);
});

test('shared input validation rejects impossible or unsafe inputs', () => {
  assert.throws(() => poker.exactHuEquityVsKnownHand(['Ah', 'Ad'], ['Kh', 'Kh'], ['2c', '7d', '9s', 'Jc', 'Qh']), /duplicate/);
  assert.throws(() => poker.comboEhsTableVsRange(['2c', '2c', '7d'], { indices: [45, 46], weights: [1] }, { trials: 1, seed: 42 }), /duplicate/);
  assert.throws(() => poker.wilsonScoreInterval(4294967297, 10, 1.96), /successes must be an integer/);
  assert.throws(() => poker.wilsonScoreInterval(1.5, 10, 1.96), /successes must be an integer/);
  assert.throws(() => poker.applySuitPermToCards(['Ah'], [NaN, 1, 2, 3]), /suit perm entry/);
  assert.throws(() => poker.icmHarvilleSkillAdjustedPayouts([NaN, 1], [100, 0], [1, 1], 0), /stacks must be finite/);
  assert.throws(() => poker.icmHarvilleSkillAdjustedPayouts([1, 1], [-100, 0], [1, 1], 0), /payouts must be finite/);
  assert.throws(() => poker.netPotAfterCallAndRake(-100, 150, 0.05, 5), /potBeforeCall/);
  assert.throws(() => poker.flopToTurnAtLeastOneHitUnionThreeCategories(47, 1, 1, 1, 2, 0, 0, 0), /inconsistent/);
  assert.throws(() => poker.flopToTurnAtLeastOneHitUnionFourCategories(47, 2, 2, 2, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), /inconsistent/);
  assert.throws(() => encode.cardStringToDeckIndex({ toString: () => 'Ah' }), /must be a string/);
  assert.throws(() => poker.binomialProportionCiWidth(3, 2 ** 32 + 10, 1.96), /nTrials must be an integer/);
  // Consistent inclusion-exclusion inputs still work.
  near(poker.flopToTurnAtLeastOneHitUnionThreeCategories(47, 9, 8, 6, 2, 1, 0, 0), (9 + 8 + 6 - 2 - 1) / 47, 1e-12, 'union');
  // Bot config: declared bounds and float representability.
  assert.equal(poker.normalizeBotConfig({ riskTolerance: 10 }).riskTolerance, 1);
  assert.throws(() => poker.normalizeBotConfig({ opponentAggressionWeight: 1e300 }), /32-bit float/);
});

test('nuts existence checks agree with full-holding enumeration and skip the redundant work', () => {
  const [wrap, ms] = timed(() => poker.omahaWrapDrawOuts(['Kc', 'Qd', '9c', '8d'], ['Jh', 'Ts', '2c']));
  assert.deepEqual(wrap, { outs: 20, nutOuts: 14 });
  assert.ok(ms < 1000, `${ms} ms`);
  // Brute force over every 4-card holding of a small live pool.
  const hero = ['Ah', 'Kh', 'Qd', 'Jd'];
  const board = ['Th', '9h', '2c', '3s'];
  const keep = ['Jh', '8h', '7c', '6c', 'Ac', 'Kc', 'Qc', '4d', '5d', '8s'];
  const dead = DECK.filter((c) => !hero.includes(c) && !board.includes(c) && !keep.includes(c));
  const heroStrength = poker.evaluateOmahaBestHand(hero, board).strength;
  let beaten = false;
  for (let a = 0; a < keep.length; a++) for (let b = a + 1; b < keep.length; b++)
    for (let c = b + 1; c < keep.length; c++) for (let d = c + 1; d < keep.length; d++)
      if (poker.evaluateOmahaBestHand([keep[a], keep[b], keep[c], keep[d]], board).strength > heroStrength) beaten = true;
  assert.equal(poker.omahaNutsOnBoard(hero, board, dead), !beaten);
  // With fewer than four live cards no villain holding exists.
  assert.equal(poker.omahaNutsOnBoard(hero, board, DECK.filter((c) => !hero.includes(c) && !board.includes(c) && c !== 'Jh')), true);
});

test('runout vulnerability equals a brute-force count on a turn', () => {
  const hero = ['Ah', 'Kh'];
  const board = ['2h', '7h', '9c', 'Qd'];
  let nuts = 0, dominated = 0, runouts = 0;
  for (const r of DECK.filter((c) => !hero.includes(c) && !board.includes(c))) {
    const full = [...board, r];
    const hs = poker.evaluateHandStrength(hero, full);
    const live = DECK.filter((c) => !hero.includes(c) && !full.includes(c));
    let best = 0;
    for (let i = 0; i < live.length; i++) for (let j = i + 1; j < live.length; j++) best = Math.max(best, poker.evaluateHandStrength([live[i], live[j]], full));
    runouts++;
    if (hs >= best) nuts++;
    if (best > hs) dominated++;
  }
  const v = poker.exactHeroRunoutVulnerability(hero, board);
  assert.equal(v.runoutCount, runouts);
  near(v.pNuts, nuts / runouts, 1e-15, 'pNuts');
  near(v.pDominated, dominated / runouts, 1e-15, 'pDominated');
});

test('bounded checks for exports the crash sweep allows to run long', () => {
  // Preflop matrix: complementary entries, at a small trial count.
  const m = poker.buildPreflopEquityMatrix({ iterations: 20, seed: 1 });
  assert.equal(m.length, 169 * 169);
  for (const [i, j] of [[0, 168], [5, 77], [166, 165]]) near(m[i * 169 + j] + m[j * 169 + i], 1, 1e-12, `M[${i},${j}] + M[${j},${i}]`);
  // Jam-stack searches: flop to river only (preflop would enumerate every board per step).
  const river = ['2c', '7d', '9s', 'Jc', 'Qh'];
  assert.ok(Number.isInteger(poker.chubukovMaxSymmetricJamStackFromHandBinarySearch(['Ah', 'Ad'], river, 1.5, 100)));
  assert.throws(() => poker.chubukovMaxSymmetricJamStackFromHandBinarySearch(['Ah', 'Ad'], [], 1.5, 100), /board of 3 to 5/);
  assert.throws(() => poker.chubukovMaxSymmetricJamStackBinarySearch(['Ah', 'Ad'], [], 1.5, 100), /board of 3 to 5/);
});
