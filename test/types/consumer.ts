// Compile-only check that the published typings work for TypeScript consumers.
import poker = require('../../index');
import type { CardInput, NativePokerState, HandEvalResult, OmahaHiLoEquity } from '../../index';
import encode = require('../../encode');

const hole: CardInput = ['Ah', 'Kh'];
const equity: number = poker.simulateHandOutcome(hole, ['Qh', 'Jh', '2c'], 1000, 42);
const best: HandEvalResult = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th']);
const hiLo: OmahaHiLoEquity = poker.exactHuOmahaHiLoEquity(['Ah', '2h', '3c', 'Kd'], ['Ks', 'Kc', 'Qs', 'Jd'], []);
const placement: number[] | Float64Array = poker.icmHarvillePlacementProbabilities([3, 2, 1]);
// @ts-expect-error the matrix is flat, not nested
const nested: number[][] = poker.icmHarvillePlacementProbabilities([3, 2, 1]);
const state: NativePokerState = { players: [], communityCards: [], phase: 'preflop', actedThisStreet: [] };
const bytes: Uint8Array = encode.packPokerState(state);

export { equity, best, hiLo, placement, nested, bytes };
