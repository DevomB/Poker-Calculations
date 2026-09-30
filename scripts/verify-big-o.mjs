/**
 * Big O (5-card PLO) export checks.
 * Run from NPM/: `node scripts/verify-big-o.mjs` (requires built native addon).
 *
 * Spot: As Ac Kh Kd 2s vs 2h 3h 4c 5d 9c on Ah 9s 8d 7c 6s
 * Hero trips aces (As Ac + Ah + K + 9). Villain 8-high straight: 4c 5d + 6s 7c 8d.
 * 5-hole vs drop: 9s 8s 2c 3d Ah on 7s 6s 5s 4c 2h — 9s is required for the flush.
 */
import { createRequire } from 'node:module';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(join(root, 'package.json'));

let poker;
try {
  poker = require('./index.js');
} catch (e) {
  console.error('Native addon not loaded — run `npm run build:native` first.');
  console.error(e.message);
  process.exit(1);
}

const EPS = 1e-9;
function assertNear(label, actual, expected, tol = EPS) {
  if (Math.abs(actual - expected) > tol) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}
function assertTrue(label, cond) {
  if (!cond) throw new Error(label);
}
function assertThrows(label, fn) {
  let threw = false;
  try {
    fn();
  } catch {
    threw = true;
  }
  if (!threw) throw new Error(`${label}: expected throw`);
}

const names = [
  'evaluateBigOBestHand',
  'evaluateBigOHandStrength',
  'exactHuBigOEquityVsKnown',
  'simulateBigOEquityVsRandom',
  'simulateBigOEquityVsRange',
  'bigOComboCount',
  'bigONutsOnBoard',  'bigOMultiwayEquityMc',
];
for (const n of names) {
  assertTrue(`${n} exported`, typeof poker[n] === 'function');
}

const hero = ['As', 'Ac', 'Kh', 'Kd', '2s'];
const villain = ['2h', '3h', '4c', '5d', '9c'];
const board = ['Ah', '9s', '8d', '7c', '6s'];

const heroEv = poker.evaluateBigOBestHand(hero, board);
const vilEv = poker.evaluateBigOBestHand(villain, board);
assertTrue('villain rank is straight', vilEv.rank === 'straight');
assertTrue('hero weaker than villain', heroEv.strength < vilEv.strength);
assertTrue(
  'strength encoding matches evaluateBigOHandStrength',
  poker.evaluateBigOHandStrength(villain, board) === vilEv.strength,
);

const fiveFlush = ['9s', '8s', '2c', '3d', 'Ah'];
const flushBoard = ['7s', '6s', '5s', '4c', '2h'];
const fiveEv = poker.evaluateBigOBestHand(fiveFlush, flushBoard);
const dropped = poker.evaluateOmahaBestHand(['8s', '2c', '3d', 'Ah'], flushBoard);
assertTrue('5-hole eval differs from dropping 9s', fiveEv.strength !== dropped.strength);
assertTrue('9s is used in the Big O flush', fiveEv.strength > dropped.strength);

const eqHero = poker.exactHuBigOEquityVsKnown(hero, villain, board);
const eqVil = poker.exactHuBigOEquityVsKnown(villain, hero, board);
assertNear('AAKK2s loses to 23459c on this river', eqHero, 0);
assertNear('23459c wins vs AAKK2s on this river', eqVil, 1);
assertNear('HU equities sum to 1', eqHero + eqVil, 1);

assertThrows('duplicate hole cards throw', () =>
  poker.evaluateBigOBestHand(['As', 'As', 'Kh', 'Kd', '2s'], board),
);
assertThrows('hole/board overlap throw', () =>
  poker.evaluateBigOBestHand(['As', 'Ac', 'Kh', 'Kd', 'Ah'], board),
);

const royalHero = ['Ah', 'Th', '9s', '8d', '7c'];
const royalBoard = ['Kh', 'Qh', 'Jh', '2c', '3d'];
const extra = ['4h', '4s', '4d', '4c', '5h', '5s', '5c', 'Tc', 'Td', 'Ts', 'Jc', 'Jd', 'Js', 'Qc', 'Qd'];
assertTrue('royal flush is nuts', poker.bigONutsOnBoard(royalHero, royalBoard) === true);
assertTrue(
  'AAKK2s is not nuts on the wrap-straight board',
  poker.bigONutsOnBoard(hero, board) === false,
);

const third = ['2d', '3c', '4s', 'Ks', 'Qc'];
const mw = poker.bigOMultiwayEquityMc([hero, villain, third], board, 40, 1);
assertNear('multiway sum ~1', mw.reduce((a, b) => a + b, 0), 1, 1e-9);
assertNear('multiway villain scoops river', mw[1], 1);

const C47_5 = (47 * 46 * 45 * 44 * 43) / 120;
assertNear('bigOComboCount hero-only dead', poker.bigOComboCount(hero), C47_5);
assertThrows('bigOComboCount duplicate dead', () => poker.bigOComboCount(['As', 'As']));

const flop = ['6c', '7d', '8s'];
const wrapHero = ['9h', 'Th', 'Jc', 'Qd', '2s'];

const flopEq = poker.evaluateBigOBestHand(wrapHero, flop);
assertTrue('flop eval has rank', typeof flopEq.rank === 'string');

const ranks = '23456789TJQKA';
const suits = 'cdhs';
function deckId(card) {
  const r = ranks.indexOf(card[0]);
  const s = suits.indexOf(card[1]);
  if (r < 0 || s < 0) throw new Error(`bad card ${card}`);
  return r * 4 + s;
}
const packed = Uint8Array.from(villain.map(deckId));
const vsRange = poker.simulateBigOEquityVsRange(hero, board, { packed }, 20, 7);
assertNear('range MC vs the river winner is 0', vsRange, 0);
const vsRand = poker.simulateBigOEquityVsRandom(hero, board, 80, 3);
assertTrue('random MC in [0,1]', vsRand >= 0 && vsRand <= 1);

const turnBoard = ['Ah', '9s', '8d', '7c'];
const turnEq = poker.exactHuBigOEquityVsKnown(hero, villain, turnBoard);
const turnFlip = poker.exactHuBigOEquityVsKnown(villain, hero, turnBoard);
assertNear('turn HU equities sum to 1', turnEq + turnFlip, 1, 1e-12);

console.log('OK: verify-big-o.mjs — 5-hole differs from PLO drop; dups throw; nuts bool; equities sum 1.');
