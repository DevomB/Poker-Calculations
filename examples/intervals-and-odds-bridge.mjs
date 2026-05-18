/**
 * FEATURES_ADDED: Stats intervals (Agresti–Coull, Wald), Hoeffding trial bound, pot-odds display ↔
 * breakeven equity, reduced-fraction pot odds, equity ↔ winning odds-against, normalized stack shares,
 * compact card parse / canonical, preflop combo list sum, ICM last-place, side-pot layer total.
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Intervals & odds bridge ---');

const ac = poker.agrestiCoullInterval(2, 10, 1.96);
console.log('agrestiCoullInterval(2, 10, 1.96):', ac);
const nw = poker.normalWaldBinomialInterval(2, 10, 1.96);
console.log('normalWaldBinomialInterval(2, 10, 1.96):', nw);
console.log('monteCarloTrialsForHoeffdingBound(0.05, 0.05):', poker.monteCarloTrialsForHoeffdingBound(0.05, 0.05));

const r = poker.potOddsRatioDisplay(100, 50);
console.log('potOddsRatioDisplay(100,50):', r, '→ breakeven equity', poker.breakevenCallEquityFromPotOddsDisplayRatio(r));
console.log('formatPotOddsReducedFraction(100,50):', poker.formatPotOddsReducedFraction(100, 50));

const eq = 0.25;
const oa = poker.equityToWinningOddsAgainst(eq);
console.log('equityToWinningOddsAgainst(0.25):', oa, '→', poker.winningOddsAgainstToEquity(oa));

console.log('normalizedStackFractions([100,300]):', poker.normalizedStackFractions([100, 300]));
console.log('parseCompactCardList("AhKh"):', poker.parseCompactCardList('AhKh'));
console.log('canonicalCardString("10h"):', poker.canonicalCardString('10h'));
console.log('preflopCombosFromNotationsList(["AA","AKs"]):', poker.preflopCombosFromNotationsList(['AA', 'AKs']));
console.log('handRankCategoryOrder("flush"):', poker.handRankCategoryOrder('flush'));

const stacks = [400, 300, 300];
console.log('icmLastPlaceProbabilitiesHarville:', poker.icmLastPlaceProbabilitiesHarville(stacks));

const ladder = poker.sidePotLadderFromCommitments([50, 100, 150]);
console.log('sidePotLayersTotalChips(ladder):', poker.sidePotLayersTotalChips(ladder));
