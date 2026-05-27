'use strict';

/** @typedef {number} Card52 Deck id 0..51: rank*4+suit (rank 0=2..12=A, suit 0=c..3=s). */

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';

/**
 * @param {string} s
 * @returns {Card52}
 */
function cardStringToDeckIndex(s) {
  const raw = String(s).trim();
  if (!raw) {
    throw new Error('invalid card string');
  }
  let rank = -1;
  let i = 0;
  if (raw.length >= 3 && raw[0] === '1' && raw[1] === '0') {
    rank = 8;
    i = 2;
  } else if (raw.length >= 2) {
    const r = raw[0].toUpperCase();
    const idx = RANKS.indexOf(r);
    if (idx >= 0) {
      rank = idx;
    }
    i = 1;
  }
  if (rank < 0 || i >= raw.length) {
    throw new Error(`invalid card string: ${s}`);
  }
  const su = raw[i].toLowerCase();
  const suit = SUITS.indexOf(su);
  if (suit < 0) {
    throw new Error(`invalid card string: ${s}`);
  }
  return rank * 4 + suit;
}

/**
 * @param {Card52} idx
 * @returns {string}
 */
function deckIndexToCardString(idx) {
  if (!Number.isInteger(idx) || idx < 0 || idx > 51) {
    throw new Error('deck index must be integer 0..51');
  }
  const rank = Math.floor(idx / 4);
  const suit = idx % 4;
  return RANKS[rank] + SUITS[suit];
}

/**
 * @param {string[]} strings
 * @returns {Uint8Array}
 */
function packCards(strings) {
  const out = new Uint8Array(strings.length);
  for (let i = 0; i < strings.length; i++) {
    out[i] = cardStringToDeckIndex(strings[i]);
  }
  return out;
}

/**
 * @param {Uint8Array} packed
 * @returns {string[]}
 */
function unpackCards(packed) {
  const out = new Array(packed.length);
  for (let i = 0; i < packed.length; i++) {
    out[i] = deckIndexToCardString(packed[i]);
  }
  return out;
}

module.exports = {
  cardStringToDeckIndex,
  deckIndexToCardString,
  packCards,
  unpackCards,
};
