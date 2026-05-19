/**
 * In-house fast evaluator benchmark (poker-calculations-forge).
 * Run: node examples/centurion-forge-benchmark.mjs
 */
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const poker = require('poker-calculations');

const bench = poker.benchmarkEvaluatorThroughput(100000);
console.log('implementation:', bench.implementation);
console.log('legacy evals/sec:', Math.round(bench.legacyEvalsPerSecond));
console.log('forge evals/sec:', Math.round(bench.fastEvalsPerSecond));
console.log(
  'speedup:',
  (bench.fastEvalsPerSecond / bench.legacyEvalsPerSecond).toFixed(2) + 'x'
);
