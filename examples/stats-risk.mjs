/**
 * FEATURES_ADDED: Stats & risk — monteCarloStandardError, wilsonScoreInterval,
 * riskOfRuinDiffusionApprox, bankrollForTargetRorDiffusion, betaBinomialFoldPosterior
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('../index.js');

console.log('\n--- Stats & risk ---');

const pHat = 0.42;
const nTrials = 5000;
console.log(`monteCarloStandardError(${pHat}, ${nTrials}):`, poker.monteCarloStandardError(pHat, nTrials));

const wilson = poker.wilsonScoreInterval(210, 500, 1.96);
console.log('wilsonScoreInterval(210 successes / 500 trials, z=1.96):', wilson);

const mu = 0.02;
const sigma2 = 4;
const B = 50;
console.log('riskOfRuinDiffusionApprox:', poker.riskOfRuinDiffusionApprox(mu, sigma2, B));
console.log(
  'bankrollForTargetRorDiffusion (target 5%):',
  poker.bankrollForTargetRorDiffusion(mu, sigma2, 0.05)
);

const beta = poker.betaBinomialFoldPosterior(2, 2, 12, 8);
console.log('betaBinomialFoldPosterior prior Beta(2,2), 12 folds / 8 calls:', beta);
