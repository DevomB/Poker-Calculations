/**
 * FEATURES_ADDED: Side pots — sidePotLadderFromCommitments, layeredPotChipEvFromEquities
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Side pots ---');

// Three players commit 30, 80, 200 → main + side layers
const ladder = poker.sidePotLadderFromCommitments([30, 80, 200]);
console.log('sidePotLadderFromCommitments [30,80,200]:', JSON.stringify(ladder, null, 2));

const layerPots = ladder.map((L) => L.potChips);
// equityPlayerByLayer[playerIndex][layerIndex]; each layer column sums to 1
const equities = [
  [0.4, 0.2, 0.1],
  [0.35, 0.5, 0.3],
  [0.25, 0.3, 0.6],
];
const chipEv = poker.layeredPotChipEvFromEquities(layerPots, equities);
console.log('layeredPotChipEvFromEquities:', chipEv);
