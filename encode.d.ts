/** Deck id 0..51: rank*4+suit (rank 0=2..12=A, suit 0=c..3=s). */
export type Card52 = number;

export function cardStringToDeckIndex(s: string): Card52;
export function deckIndexToCardString(idx: Card52): string;
export function packCards(strings: string[]): Uint8Array;
export function unpackCards(packed: Uint8Array): string[];

import type { NativePokerState } from './index';

/** PKST packed state bytes via native `encodePokerState`. */
export function packPokerState(state: NativePokerState): Uint8Array;
/** Round-trip to `NativePokerState` via native `decodePokerState`. */
export function unpackPokerState(bytes: Uint8Array): NativePokerState;
