/**
 * Shared loader and assertions for scripts/verify-*.mjs. `npm test` runs every verify script
 * (test/verify.test.js); run one directly with `node scripts/verify-omaha.mjs`.
 */
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(join(root, 'package.json'));

let loaded;
try {
  loaded = require('./index.js');
} catch (e) {
  throw new Error(`Native addon not loaded: run \`npm run build:native\` and stage a prebuild.\n${e.message}`);
}
export const poker = loaded;

export const EPS = 1e-9;

/** `actual` within `tol` of `expected`. NaN fails (a plain `Math.abs(a - e) > tol` check lets it through). */
export function assertNear(label, actual, expected, tol = EPS) {
  if (Object.is(actual, expected)) return; // exact match, including equal infinities
  if (!(Math.abs(actual - expected) <= tol)) {
    throw new Error(`${label}: expected ${expected} (tolerance ${tol}), got ${actual}`);
  }
}

export function assertTrue(label, cond) {
  if (!cond) throw new Error(label);
}

export function assertThrows(label, fn) {
  try {
    fn();
  } catch {
    return;
  }
  throw new Error(`${label}: expected throw`);
}

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';

/** Deck id `rank * 4 + suit` of a two-character card string. */
export function deckId(card) {
  const r = RANKS.indexOf(card[0]);
  const s = SUITS.indexOf(card[1]);
  if (r < 0 || s < 0) throw new Error(`bad card ${card}`);
  return r * 4 + s;
}

/** Dense 1326 slot of two deck ids, either order (same formula as include/poker/range.hpp). */
export function comboIndex(a, b) {
  const lo = Math.min(a, b);
  const hi = Math.max(a, b);
  return lo * 51 - (lo * (lo - 1)) / 2 + (hi - lo - 1);
}

/** Sparse range spec from `[cardA, cardB, weight?]` triples. */
export function sparseHoles(pairs) {
  const indices = [];
  const weights = [];
  for (const [a, b, w] of pairs) {
    indices.push(deckId(a), deckId(b));
    weights.push(w ?? 1);
  }
  return { indices: Int32Array.from(indices), weights: Float64Array.from(weights) };
}
