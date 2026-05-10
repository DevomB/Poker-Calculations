'use strict';

/**
 * Pure JS odds/stack helpers (SPR, MDF, rule of 2 & 4, …).
 * Monte Carlo and hand ordering live in the native addon — keep this module allocation-free and obvious.
 */

function assertNonNegFinite(name, x) {
  if (typeof x !== 'number' || !Number.isFinite(x) || x < 0) {
    throw new TypeError(`${name} must be a finite non-negative number`);
  }
}

function assertPositiveFinite(name, x) {
  if (typeof x !== 'number' || !Number.isFinite(x) || x <= 0) {
    throw new TypeError(`${name} must be a finite positive number`);
  }
}

function clamp01(x) {
  return Math.max(0, Math.min(1, x));
}

/** Stack-to-pot ratio: effective stack divided by pot. */
function spr(potChips, effectiveStackChips) {
  assertNonNegFinite('potChips', potChips);
  assertNonNegFinite('effectiveStackChips', effectiveStackChips);
  if (potChips === 0) {
    return effectiveStackChips === 0 ? 0 : Infinity;
  }
  return effectiveStackChips / potChips;
}

/** Minimum of stacks (two-player effective stack when both cover). */
function effectiveStack(...stacks) {
  if (stacks.length === 0) {
    return 0;
  }
  let m = stacks[0];
  assertNonNegFinite('stack', m);
  for (let i = 1; i < stacks.length; i++) {
    const x = stacks[i];
    assertNonNegFinite('stack', x);
    if (x < m) {
      m = x;
    }
  }
  return m;
}

/**
 * Equity needed to break even on a pure chip call: toCall / (potBeforeCall + toCall).
 */
function breakevenCallEquity(potBeforeCall, toCall) {
  assertNonNegFinite('potBeforeCall', potBeforeCall);
  assertNonNegFinite('toCall', toCall);
  const denom = potBeforeCall + toCall;
  if (denom <= 0 || toCall === 0) {
    return 0;
  }
  return toCall / denom;
}

/** MDF: pot / (pot + bet) after villain bets into `potBeforeOpponentBet`. */
function minimumDefenseFrequency(potBeforeOpponentBet, opponentBetSize) {
  assertNonNegFinite('potBeforeOpponentBet', potBeforeOpponentBet);
  assertNonNegFinite('opponentBetSize', opponentBetSize);
  const denom = potBeforeOpponentBet + opponentBetSize;
  return denom <= 0 ? 0 : potBeforeOpponentBet / denom;
}

function stackInBigBlinds(stackChips, bigBlind) {
  assertNonNegFinite('stackChips', stackChips);
  assertPositiveFinite('bigBlind', bigBlind);
  return stackChips / bigBlind;
}

/** Pot odds as pot : toCall (e.g. 3.5 ⇒ 3.5:1). */
function potOddsRatioDisplay(potBeforeCall, toCall) {
  assertNonNegFinite('potBeforeCall', potBeforeCall);
  assertNonNegFinite('toCall', toCall);
  return toCall === 0 ? Infinity : potBeforeCall / toCall;
}

function formatPotOdds(potBeforeCall, toCall, decimals = 2) {
  assertNonNegFinite('potBeforeCall', potBeforeCall);
  assertNonNegFinite('toCall', toCall);
  if (toCall === 0) {
    return '∞:1';
  }
  const r = potBeforeCall / toCall;
  const scale = 10 ** decimals;
  const f = Math.round(r * scale) / scale;
  return `${f}:1`;
}

/** Rule of 4 — two cards to come; outs capped defensively. */
function ruleOfFourEquity(outs) {
  assertNonNegFinite('outs', outs);
  const o = Math.min(outs, 48);
  return Math.min(1, (o * 4) / 100);
}

/** Rule of 2 — one card to come. */
function ruleOfTwoEquity(outs) {
  assertNonNegFinite('outs', outs);
  const o = Math.min(outs, 48);
  return Math.min(1, (o * 2) / 100);
}

/**
 * Implied-odds breakeven: average extra future win (beyond current pot + call) so a call is neutral.
 * Returns Infinity if equity is 0.
 */
function impliedBreakevenFutureWin(potBeforeCall, toCall, equity) {
  assertNonNegFinite('potBeforeCall', potBeforeCall);
  assertNonNegFinite('toCall', toCall);
  if (typeof equity !== 'number' || !Number.isFinite(equity)) {
    throw new TypeError('equity must be a finite number');
  }
  const e = clamp01(equity);
  if (e <= 0) {
    return Infinity;
  }
  const immediateWinTotal = potBeforeCall + toCall;
  return Math.max(0, ((1 - e) * toCall) / e - immediateWinTotal);
}

/** Polarized river ratio Bet / (Pot + 2×Bet). */
function bluffToValueRatio(potBeforeBet, betSize) {
  assertNonNegFinite('potBeforeBet', potBeforeBet);
  assertNonNegFinite('betSize', betSize);
  const denom = potBeforeBet + 2 * betSize;
  return denom <= 0 ? 0 : betSize / denom;
}

module.exports = {
  spr,
  effectiveStack,
  breakevenCallEquity,
  minimumDefenseFrequency,
  stackInBigBlinds,
  potOddsRatioDisplay,
  formatPotOdds,
  ruleOfFourEquity,
  ruleOfTwoEquity,
  impliedBreakevenFutureWin,
  bluffToValueRatio,
};
