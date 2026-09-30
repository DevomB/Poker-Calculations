// Argument synthesis for the crash sweep: turns (function signature, scenario) into an argv.
// Values are picked from parameter names and index.d.ts types, so a new export is covered automatically.

export const SCENARIOS = [
  'flop',
  'turn',
  'river',
  'preflop',
  'duplicateCards',
  'invalidCards',
  'tooManyBoardCards',
  'empty',
  'zero',
  'negative',
  'nan',
  'huge',
  'wrongTypes',
  'noArgs',
  'oneHoleCard',
  'shortBoard',
  'manyPlayers',
];

const HOLE = ['Ah', 'Kh'];
const BOARD = { preflop: [], flop: ['Qh', 'Jd', '2c'], turn: ['Qh', 'Jd', '2c', '3d'], river: ['Qh', 'Jd', '2c', '3d', '4s'] };
const VILLAINS = [['Qs', 'Qd'], ['7c', '6c'], ['9d', '8d']];
const OMAHA_HOLE = ['Ah', 'Kh', 'Ad', 'Kd'];
const OMAHA_VILLAIN = ['Qs', 'Qc', 'Ts', 'Tc'];
const BIGO_HOLE = ['Ah', 'Kh', 'Ad', 'Kd', '5c'];
const BIGO_VILLAIN = ['Qs', 'Qc', 'Ts', 'Tc', '6h'];
const SD_HOLE = ['Ah', 'Kh'];
const SD_BOARD = { preflop: [], flop: ['Qh', 'Jd', '7c'], turn: ['Qh', 'Jd', '7c', '8d'], river: ['Qh', 'Jd', '7c', '8d', '9s'] };
const STUD7 = ['Ah', 'Kh', 'Qh', 'Jh', '9c', '8d', '2s'];
const DEUCE5 = ['7h', '5d', '4c', '3s', '2h'];

const isCount = (n) =>
  /(sim|trial|iter|sample|rollout|thread|budget|step|^n[A-Z]|^num|count|rounds|epochs|buckets|maxDepth)/i.test(n);
const isSeed = (n) => /seed/i.test(n);
const isProb = (n) => /(prob|equity|freq|fold|fraction|share|rate|alpha|^p[A-Z]?$|pct|percent|weight|skill|ehs|confidence|tolerance|eps)/i.test(n);
const isPlayers = (n) => /(villain|player|opponent|seats|entrants|places|hole_n|numHands)/i.test(n);
const isIndex = (n) => /(index|deckId|position|seat$|hero$|heroSeat|category)/i.test(n);

function numberFor(name, sc) {
  switch (sc) {
    case 'zero':
    case 'empty':
      return 0;
    case 'negative':
      return -1;
    case 'nan':
      return Number.NaN;
    case 'huge':
      return isCount(name) ? 2000 : 1e12;
    default:
      if (isSeed(name)) return 42;
      if (isCount(name)) return /thread/i.test(name) ? 2 : 200;
      if (isProb(name)) return 0.5;
      if (isPlayers(name)) return sc === 'manyPlayers' ? 30 : 2;
      if (isIndex(name)) return 1;
      return 100;
  }
}

function variant(fn) {
  if (/bigO/i.test(fn)) return 'bigO';
  if (/omaha|plo/i.test(fn)) return 'omaha';
  if (/shortDeck/i.test(fn)) return 'shortDeck';
  if (/stud|razz/i.test(fn)) return 'stud';
  if (/deuceSeven/i.test(fn)) return 'deuce';
  return 'holdem';
}

function street(sc) {
  if (sc === 'turn') return 'turn';
  if (sc === 'river' || sc === 'negative' || sc === 'nan' || sc === 'zero') return 'river';
  if (sc === 'preflop') return 'preflop';
  return 'flop';
}

function cardsFor(fn, pname, sc) {
  const v = variant(fn);
  if (sc === 'empty') return [];
  if (sc === 'invalidCards') return ['Zz', '1x'];
  const isBoard = /board|community|runout|flop|turn|river/i.test(pname);
  const isDead = /dead|muck|removed|blocker|exclude/i.test(pname);
  const isVillain = /villain|opponent|other|cardsB|hand1|hand2|hand3|b$/i.test(pname) && !isBoard;
  const st = street(sc);
  if (sc === 'duplicateCards') return isBoard ? ['Ah', 'Kh', '2c'] : ['Ah', 'Ah'];
  if (isDead) return ['2d'];
  if (sc === 'oneHoleCard' && !isBoard) return ['Ah'];
  if (sc === 'shortBoard' && isBoard) return ['Qh'];
  if (isBoard) {
    if (sc === 'tooManyBoardCards') return ['Qh', 'Jd', '2c', '3d', '4s', '5s'];
    return v === 'shortDeck' ? SD_BOARD[st] : BOARD[st];
  }
  if (v === 'stud') return STUD7;
  if (v === 'deuce') return DEUCE5;
  if (v === 'omaha') return isVillain ? OMAHA_VILLAIN : OMAHA_HOLE;
  if (v === 'bigO') return isVillain ? BIGO_VILLAIN : BIGO_HOLE;
  if (/^cards$|cardsA|best/i.test(pname) && v === 'holdem') return [...HOLE, ...BOARD[st]];
  return isVillain ? VILLAINS[0] : v === 'shortDeck' ? SD_HOLE : HOLE;
}

function vectorFor(pname, sc) {
  if (sc === 'empty') return [];
  if (sc === 'nan') return [Number.NaN, 1, 2];
  if (sc === 'negative') return [-1, 5, 3];
  if (sc === 'zero') return [0, 0, 0];
  if (sc === 'huge') return [1e12, 1e12, 1];
  if (/payout|prize/i.test(pname)) return [50, 30, 20];
  if (/equit|prob|weight|freq/i.test(pname)) return [0.5, 0.3, 0.2];
  if (/bount/i.test(pname)) return [10, 10, 10];
  return [3000, 2000, 1000];
}

function rangeFor(sc) {
  if (sc === 'empty') return new Float64Array(0);
  if (sc === 'zero') return new Float64Array(1326);
  if (sc === 'nan') return new Float64Array(1326).fill(Number.NaN);
  if (sc === 'negative') return new Float64Array(1326).fill(-1);
  return new Float64Array(1326).fill(1);
}

function valueFor(fn, p, sc) {
  const t = p.type;
  const n = p.name;
  if (sc === 'wrongTypes') return typeof t === 'string' && t.startsWith('number') ? 'x' : 12345;
  if (t === 'number' || t === '0 | 1' || t === '0 | 1 | 2' || t.startsWith('number |') || t === 'string | number') {
    if (t.startsWith('0 |')) return 1;
    return numberFor(n, sc);
  }
  if (t === 'CardInput' || t === 'string[]' || t === 'CardInput | null') return cardsFor(fn, n, sc);
  if (t === 'CardInput[]') {
    if (sc === 'empty') return [];
    if (sc === 'duplicateCards') return [['Ah', 'Kh'], ['Ah', 'Kh']];
    const v = variant(fn);
    if (v === 'omaha') return [OMAHA_HOLE, OMAHA_VILLAIN];
    if (v === 'bigO') return [BIGO_HOLE, BIGO_VILLAIN];
    return [HOLE, ...VILLAINS.slice(0, 2)];
  }
  if (/SparseRangeSpec|^Float64Array$/.test(t)) {
    if (/stack|payout|prize|equit|bount/i.test(n)) return Float64Array.from(vectorFor(n, sc));
    return rangeFor(sc);
  }
  if (t === 'F64VectorInput' || t === 'number[]' || t === 'F64VectorInput | number' || t === 'Int32Array | number[]') return vectorFor(n, sc);
  if (t === 'number[][] | Float64Array' || t === 'F64VectorInput | number[][]') {
    if (sc === 'empty') return [];
    return [[0.5, 0.5], [0.5, 0.5]];
  }
  if (t === 'Uint8Array') return sc === 'empty' ? new Uint8Array(0) : new Uint8Array([12, 25, 38, 51, 0, 1]);
  if (t === 'Uint32Array') return sc === 'empty' ? new Uint32Array(0) : new Uint32Array([2, 3, 1, 200, 42]);
  if (t === 'string') {
    if (sc === 'empty') return '';
    if (/card/i.test(n)) return sc === 'invalidCards' ? 'Zz' : 'Ah';
    if (/text|list/i.test(n)) return 'AhKhQh';
    return sc === 'invalidCards' ? '??' : 'AKs';
  }
  if (t === 'boolean' || t.startsWith('boolean')) return true;
  if (t === 'SimBatchSpec[]') return sc === 'empty' ? [] : [{ holeCards: HOLE, board: BOARD.flop, numSimulations: 100, seed: 1 }];
  if (t === 'RangeNotationWeight[]') return [{ notation: 'AKs', weight: 1 }];
  if (t === 'SidePotLayer[]') return sc === 'empty' ? [] : [{ amount: 100, eligible: [0, 1] }];
  if (/PokerStateBytes|NativePokerState/.test(t)) {
    if (t.startsWith('Array')) return [new Uint8Array(8)];
    return sc === 'empty' ? new Uint8Array(0) : new Uint8Array([80, 75, 83, 84, 1, 2, 0, 0]);
  }
  if (t.startsWith('Array<')) return [null];
  // Options bags and structured inputs: an empty object exercises the defaults.
  return p.optional ? undefined : {};
}

export function argsFor(fn, params, sc) {
  if (sc === 'noArgs') return [];
  return params.map((p) => valueFor(fn, p, sc));
}
