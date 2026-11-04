// Compile-only check that the published typings work for TypeScript consumers.
import poker = require('../../index');
import type {
  CardInput,
  NativePokerState,
  HandEvalResult,
  HandEvalResultSlim,
  OmahaHiLoEquity,
  RangesEquityResult,
  DecisionDiagnosticResult,
  DecisionBatchErrorRow,
  EvaluateBestHandOptions,
} from '../../index';
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
const vsRanges: RangesEquityResult = poker.simulateEquityVsRanges(hole, ['Qh', 'Jh', '2c'], [new Float64Array(1326).fill(1)], 100, 1, { deadCards: ['2d'] });


// `{ format: 'slim' }` selects the slim result, which has no `rank` or `kickers` at runtime.
const slim: HandEvalResultSlim = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th'], { format: 'slim' });
// @ts-expect-error slim results carry no kickers
const slimKickers: number[] = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th'], { format: 'slim' }).kickers;
const full: HandEvalResult = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th'], { format: 'full' });
const omahaSlim: HandEvalResultSlim = poker.evaluateOmahaBestHand(['Ah', 'Kh', '2c', '3d'], ['Qh', 'Jh', 'Th'], { format: 'slim' });
// @ts-expect-error slim Omaha results carry no rank label
const omahaRank: string = poker.evaluateOmahaBestHand(['Ah', 'Kh', '2c', '3d'], ['Qh', 'Jh', 'Th'], { format: 'slim' }).rank;
const studSlim: HandEvalResultSlim = poker.evaluateStudBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th'], { format: 'slim' });
// A format only known at runtime gives either shape.
const anyFormat: EvaluateBestHandOptions = { format: Math.random() < 0.5 ? 'slim' : 'full' };
const either: HandEvalResult | HandEvalResultSlim = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th'], anyFormat);

// Batch rows are a decision or `{ error }`; narrowing is required before reading decision fields.
const rows = poker.runBotPolicyBatch([state], { monteCarloSimulations: 100, rngSeed: 1 });
const firstRow = rows[0];
// @ts-expect-error an error row has no decision
const unchecked = firstRow.decision;
let decided: DecisionDiagnosticResult | undefined;
let failed: DecisionBatchErrorRow | undefined;
if (firstRow.error !== undefined) {
  failed = firstRow;
} else {
  decided = firstRow;
}

export {
  equity, best, hiLo, placement, nested, bytes, vsRanges, slim, slimKickers, full, omahaSlim, omahaRank, studSlim,
  either, unchecked, decided, failed,
};
