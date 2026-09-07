/**
 * Omaha Hi (PLO) export checks.
 * Run from NPM/: `node scripts/verify-omaha.mjs` (requires built native addon).
 *
 * Spot: As Ac Kh Kd vs 2h 3h 4c 5d on Ah 9s 8d 7c 6s
 * Hero makes trip aces (As Ac + Ah + K + 9). No 5/T for a straight.
 * Villain makes 8-high straight: 4c 5d + 6s 7c 8d (exactly 2 hole + 3 board).
 * Expected winner: 2h3h4c5d.
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
  'evaluateOmahaBestHand',
  'evaluateOmahaHandStrength',
  'exactHuOmahaEquityVsKnown',
  'simulateOmahaEquityVsRandom',
  'simulateOmahaEquityVsRange',
  'omahaComboCount',
  'omahaNutsOnBoard',
  'omahaWrapDrawOuts',
  'omahaNuttednessScore',
  'omahaMultiwayEquityMc',
];
for (const n of names) {
  assertTrue(`${n} exported`, typeof poker[n] === 'function');
}

const hero = ['As', 'Ac', 'Kh', 'Kd'];
const villain = ['2h', '3h', '4c', '5d'];
const board = ['Ah', '9s', '8d', '7c', '6s'];

const heroEv = poker.evaluateOmahaBestHand(hero, board);
const vilEv = poker.evaluateOmahaBestHand(villain, board);
assertTrue('villain rank is straight', vilEv.rank === 'straight');
assertTrue('hero weaker than villain', heroEv.strength < vilEv.strength);
assertTrue(
  'strength encoding matches evaluateOmahaHandStrength',
  poker.evaluateOmahaHandStrength(villain, board) === vilEv.strength,
);

const eqHero = poker.exactHuOmahaEquityVsKnown(hero, villain, board);
const eqVil = poker.exactHuOmahaEquityVsKnown(villain, hero, board);
assertNear('AAKK loses to 2345 on this river', eqHero, 0);
assertNear('2345 wins vs AAKK on this river', eqVil, 1);
assertNear('HU equities sum to 1', eqHero + eqVil, 1);

assertThrows('duplicate hole cards throw', () =>
  poker.evaluateOmahaBestHand(['As', 'As', 'Kh', 'Kd'], board),
);
assertThrows('hole/board overlap throw', () =>
  poker.evaluateOmahaBestHand(['As', 'Ac', 'Kh', 'Ah'], board),
);

const flushHero = ['Ah', 'Kh', 'Qs', 'Jd'];
const flushBoard = ['2h', '7h', '9h', 'Td', '3s'];
assertTrue('nut flush is nuts', poker.omahaNutsOnBoard(flushHero, flushBoard) === true);
assertNear('nut flush nuttedness is 1', poker.omahaNuttednessScore(flushHero, flushBoard), 1);
assertTrue(
  'AAKK is not nuts on the wrap-straight board',
  poker.omahaNutsOnBoard(hero, board) === false,
);
assertTrue('nuttedness of AAKK in (0,1)', poker.omahaNuttednessScore(hero, board) < 1);

const third = ['2s', '3c', '4d', 'Ks'];
const mw = poker.omahaMultiwayEquityMc([hero, villain, third], board, 40, 1);
assertNear('multiway sum ~1', mw.reduce((a, b) => a + b, 0), 1, 1e-9);
assertNear('multiway villain scoops river', mw[1], 1);

const C48_4 = (48 * 47 * 46 * 45) / 24;
assertNear('omahaComboCount hero-only dead', poker.omahaComboCount(hero), C48_4);
assertThrows('omahaComboCount duplicate dead', () => poker.omahaComboCount(['As', 'As']));

const flop = ['6c', '7d', '8s'];
const wrapHero = ['9h', 'Th', 'Jc', 'Qd'];
const wrap = poker.omahaWrapDrawOuts(wrapHero, flop);
assertTrue(`wrap outs > 0 (${wrap.outs})`, wrap.outs > 0);
assertTrue('nutOuts <= outs', wrap.nutOuts <= wrap.outs);

const flopEq = poker.evaluateOmahaBestHand(wrapHero, flop);
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
const vsRange = poker.simulateOmahaEquityVsRange(hero, board, { packed }, 20, 7);
assertNear('range MC vs the river winner is 0', vsRange, 0);
const vsRand = poker.simulateOmahaEquityVsRandom(hero, board, 80, 3);
assertTrue('random MC in [0,1]', vsRand >= 0 && vsRand <= 1);

const turnBoard = ['Ah', '9s', '8d', '7c'];
const turnEq = poker.exactHuOmahaEquityVsKnown(hero, villain, turnBoard);
const turnFlip = poker.exactHuOmahaEquityVsKnown(villain, hero, turnBoard);
assertNear('turn HU equities sum to 1', turnEq + turnFlip, 1, 1e-12);

console.log('OK: verify-omaha.mjs — villain 2345 wins river; dups throw; nut flush; equities sum 1.');
