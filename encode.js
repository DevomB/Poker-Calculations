'use strict';

const path = require('path');

/** @typedef {number} Card52 Deck id 0..51: rank*4+suit (rank 0=2..12=A, suit 0=c..3=s). */

/** @returns {import('./index.js')} */
function nativeAddon() {
  return require('node-gyp-build')(path.join(__dirname));
}

const RANKS = '23456789TJQKA';
const SUITS = 'cdhs';

/**
 * @param {string} s
 * @returns {Card52}
 */
function cardStringToDeckIndex(s) {
  // Same rule as the native parser: surrounding whitespace is ignored, rank and suit are
  // case-insensitive, ten may be written "10", and nothing else may follow ("Ahjunk" is invalid).
  const m = /^(10|[2-9TJQKA])([CDHS])$/i.exec(String(s).trim());
  if (!m) {
    throw new Error(`invalid card string: ${s}`);
  }
  const rank = m[1] === '10' ? 8 : RANKS.indexOf(m[1].toUpperCase());
  return rank * 4 + SUITS.indexOf(m[2].toLowerCase());
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

/**
 * PKST packed binary state (native `encodePokerState`).
 * @param {import('./index').NativePokerState} state
 * @returns {Uint8Array}
 */
function packPokerState(state) {
  return nativeAddon().encodePokerState(state);
}

/**
 * @param {Uint8Array} bytes
 * @returns {import('./index').NativePokerState}
 */
function unpackPokerState(bytes) {
  return nativeAddon().decodePokerState(bytes);
}

module.exports = {
  cardStringToDeckIndex,
  deckIndexToCardString,
  packCards,
  unpackCards,
  packPokerState,
  unpackPokerState,
};
