/**
 * FEATURES_ADDED: ICM — icmWinProbabilitiesHarville, icmHarvillePlacementProbabilities,
 * icmExpectedPayouts, icmPairwiseBubbleFactor
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- ICM ---');

const stacks = [400, 300, 300];
const payouts = [500, 300, 200];

console.log('icmWinProbabilitiesHarville:', poker.icmWinProbabilitiesHarville(stacks));

const placement = poker.icmHarvillePlacementProbabilities(stacks);
console.log('icmHarvillePlacementProbabilities[0] (player 0 by finish rank):', placement[0]);

const ev = poker.icmExpectedPayouts(stacks, payouts);
console.log('icmExpectedPayouts:', ev);

const bubble = poker.icmPairwiseBubbleFactor(stacks, payouts, 0, 1, 50);
console.log('icmPairwiseBubbleFactor hero=0 villain=1 pot=50:', bubble);
