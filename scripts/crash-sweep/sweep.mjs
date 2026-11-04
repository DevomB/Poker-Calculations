// Crash sweep: call every export under every scenario in args.mjs, each batch in a child process,
// and report calls that kill the process (native crash or abort) or run past --timeout.
// A thrown JS error is the expected outcome for bad input and counts as a pass.
//
// Usage: npm run sweep -- [--shards 8] [--timeout 15000] [--only name,name] [--json out.json]
//
// Timeouts are classified, not ignored. A call may run past the limit only when it is an export
// listed in EXPENSIVE (valid input whose full-size work is legitimately long; each entry names the
// bounded check that covers it instead) and its arguments contain no invalid or duplicate card.
// Any other timeout fails the sweep: a cheap path hung, or invalid input was not rejected first.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { parseSignatures } from './signatures.mjs';
import { SCENARIOS, argsFor } from './args.mjs';

// Exports whose full-size work on valid synthesized input exceeds the per-call limit, why, and the
// bounded representative check that runs instead.
export const EXPENSIVE = {
  buildPreflopEquityMatrix: 'Monte Carlo over 14,365 class matchups at 5000 trials (audit-regressions: 20 trials)',
  comboEhsTableVsRange: '1326 hero combos x full range x runouts (range-consistency: 3-combo range)',
  ehs2BucketsVsRange: 'the full EHS2 table (range-consistency: 3-combo range)',
  equityDeltaIfCardRemoved: 'exact range equity from preflop or a 1-card board (range-consistency: turn)',
  exactEquityCardRemovalGradient: '52 exact range equities from early streets (range-consistency: turn)',
  exactEquityCardRemovalGradientAsync: 'same as exactEquityCardRemovalGradient',
  exactHuEquityVsRandomHand: 'exact preflop enumeration, C(50,2) x C(48,5) (fixes, partial-boards: flop)',
  exactHuEquityVsRandomHandAsync: 'same as exactHuEquityVsRandomHand',
  exactHuEquityVsRange: 'exact range equity from preflop or a 1-card board (range-consistency: turn, river)',
  fictitiousPlayRiver: 'iterations x full 1326-combo ranges (audit-regressions: analytic river game)',
  nashBlindVsBlindSolve: 'builds the 169 x 169 equity matrix at default trials (verify-nash-push-fold)',
  nashCallChart169: 'a Nash solve per stack on a grid (verify-nash-push-fold)',
  nashHeadsUpCallRange: 'builds the 169 x 169 equity matrix at default trials (verify-nash-push-fold)',
  nashHeadsUpJamCallSolve: 'builds the 169 x 169 equity matrix at default trials (audit-regressions)',
  nashHeadsUpJamRange: 'builds the 169 x 169 equity matrix at default trials (audit-regressions)',
  nashIndifferenceStackBb: 'a Nash solve per bisection step (verify-nash-push-fold)',
  nashJamFoldChart169: 'a Nash solve per stack on a grid (verify-nash-push-fold)',
  rangeRemovalSensitivityVsHero: '52 exact range equities from early streets (range-consistency: river)',
  // Below about two seconds per call these also run long on valid full-size input.
  chubukovMaxSymmetricJamStackBinarySearch: 'exact equity vs a random hand on a flop (audit-regressions: river)',
  chubukovMaxSymmetricJamStackFromHandBinarySearch: 'exact equity vs a random hand on a flop (audit-regressions: river)',
  cfrRiverBetCallFoldSolve: 'iterations x full 1326-combo ranges (audit-regressions: analytic river game)',
  solveHuRiverCheckBetTree: 'a CFR solve on full ranges (audit-regressions, verify-cfr-subgame)',
  exactHeroEquityRunoutQuantiles: '1081 flop runouts x exact equity (partial-boards)',
  exactHeroEquityRunoutQuantilesAsync: 'same as exactHeroEquityRunoutQuantiles',
  exactHeroRunoutVulnerability: 'flop runouts x every villain hand (audit-regressions: turn)',
  exactHeroRunoutVulnerabilityAsync: 'same as exactHeroRunoutVulnerability',
  exactInformationRegretVsClairvoyant: 'flop runouts x exact range equity (review-4.1.2: turn)',
  twoStreetPositivePotential: 'two-card runouts x full range (range-consistency, fixes)',
  twoStreetNegativePotential: 'two-card runouts x full range (range-consistency)',
  exactHuEquityVsKnownHand: 'exact preflop enumeration over C(48,5) boards (fixes: flop)',
  exactHuOmahaEquityVsKnown: 'exact preflop Omaha enumeration (fixes, verify-omaha)',
  exactHuOmahaHiLoEquity: 'exact preflop Omaha Hi-Lo enumeration (verify-omaha-hi-lo)',
  exactHuBigOEquityVsKnown: 'exact preflop or 1-card-board Big O enumeration (verify-big-o)',
  exactMultiwayEquityKnownHands: 'exact preflop multiway enumeration (range-consistency: turn)',
  exactMultiwayEquityWithDeadCards: 'exact preflop multiway enumeration (verify-exact-multiway)',
  exactMultiwaySidePotChipEv: 'exact preflop multiway enumeration (verify-exact-multiway)',
  exactMultiwayTieFrequency: 'exact preflop multiway enumeration (verify-exact-multiway)',
  exactMultiwayWinTieLoseKnownHands: 'exact preflop multiway enumeration (fixes)',
  shortDeckNashHuJamRange: 'builds the 81 x 81 equity matrix at default trials (audit-regressions, verify-short-deck)',
};

const CARD = /^(10|[2-9TJQKA])[CDHS]$/i;
/** True when the synthesized arguments hold a malformed or repeated card string. */
function hasInvalidCards(args) {
  const seen = new Set();
  let bad = false;
  const visit = (v) => {
    if (typeof v === 'string' && /^[0-9TJQKA]/i.test(v) && v.length <= 4) {
      if (!CARD.test(v.trim())) bad = true;
      const key = v.trim().toLowerCase().replace(/^10/, 't');
      if (seen.has(key)) bad = true;
      seen.add(key);
    } else if (Array.isArray(v)) v.forEach(visit);
  };
  args.forEach(visit);
  return bad;
}

const self = fileURLToPath(import.meta.url);
const pkgDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const argv = process.argv.slice(2);
const opt = (k, d) => {
  const i = argv.indexOf(k);
  return i >= 0 ? argv[i + 1] : d;
};

function buildJobs() {
  const sigs = parseSignatures(path.join(pkgDir, 'index.d.ts'));
  const only = opt('--only', '')?.split(',').filter(Boolean);
  const jobs = [];
  for (const [fn] of sigs) {
    if (only?.length && !only.includes(fn)) continue;
    if (/^benchmark/.test(fn)) continue; // intentionally long-running
    for (const sc of SCENARIOS) jobs.push([fn, sc]);
  }
  return { sigs, jobs };
}

if (argv[0] === '--child') {
  const shard = Number(argv[1]);
  const shards = Number(argv[2]);
  const start = Number(argv[3]);
  const { sigs, jobs } = buildJobs();
  const mine = jobs.filter((_, i) => i % shards === shard);
  const poker = createRequire(path.join(pkgDir, 'index.js'))('./index.js');
  process.on('unhandledRejection', () => {});
  const out = (s) => fs.writeSync(1, s + '\n');
  for (let k = start; k < mine.length; k++) {
    const [fn, sc] = mine[k];
    out(`B ${k}`);
    let status = 'ok';
    try {
      const r = poker[fn](...argsFor(fn, sigs.get(fn), sc));
      if (r && typeof r.then === 'function') {
        try {
          await r;
        } catch (e) {
          status = 'threw';
        }
      }
    } catch (e) {
      status = 'threw';
    }
    out(`E ${k} ${status}`);
  }
  process.exit(0);
}

const sigsAll = parseSignatures(path.join(pkgDir, 'index.d.ts'));
const shards = Number(opt('--shards', '8'));
const timeoutMs = Number(opt('--timeout', '20000'));
const { jobs } = buildJobs();
const failures = [];
const tally = { ok: 0, threw: 0 };
const t0 = Date.now();

function runShard(shard) {
  const mine = jobs.filter((_, i) => i % shards === shard);
  return new Promise((resolve) => {
    const launch = (start) => {
      if (start >= mine.length) return resolve();
      const child = spawn(process.execPath, [self, '--child', String(shard), String(shards), String(start), ...argv], {
        cwd: pkgDir,
        stdio: ['ignore', 'pipe', 'pipe'],
      });
      let current = -1;
      let buf = '';
      let stderr = '';
      let timer = null;
      let timedOut = false;
      const arm = () => {
        clearTimeout(timer);
        timer = setTimeout(() => {
          timedOut = true;
          child.kill('SIGKILL');
        }, timeoutMs);
      };
      child.stderr.on('data', (d) => (stderr = (stderr + d).slice(-2000)));
      child.stdout.on('data', (d) => {
        buf += d;
        let nl;
        while ((nl = buf.indexOf('\n')) >= 0) {
          const line = buf.slice(0, nl).trim();
          buf = buf.slice(nl + 1);
          const [tag, idx, status] = line.split(' ');
          if (tag === 'B') {
            current = Number(idx);
            arm();
          } else if (tag === 'E') {
            clearTimeout(timer);
            tally[status] = (tally[status] ?? 0) + 1;
            current = -1;
          }
        }
      });
      child.on('exit', (code, signal) => {
        clearTimeout(timer);
        if (current >= 0) {
          const [fn, sc] = mine[current];
          failures.push({
            fn,
            sc,
            kind: timedOut ? 'timeout' : 'crash',
            expected: timedOut && fn in EXPENSIVE && !hasInvalidCards(argsFor(fn, sigsAll.get(fn), sc)),
            code,
            signal,
            stderr: stderr.trim().split('\n').slice(-3).join(' | '),
          });
          launch(current + 1);
        } else resolve();
      });
    };
    launch(0);
  });
}

await Promise.all(Array.from({ length: shards }, (_, s) => runShard(s)));
const byFn = new Map();
for (const f of failures) {
  if (!byFn.has(f.fn)) byFn.set(f.fn, []);
  byFn.get(f.fn).push(f);
}
const expected = failures.filter((f) => f.expected);
const unexpected = failures.filter((f) => !f.expected);
console.log(`calls ${jobs.length}  ok ${tally.ok}  threw ${tally.threw}  crashes ${failures.filter((f) => f.kind === 'crash').length}  timeouts ${failures.filter((f) => f.kind === 'timeout').length} (expected ${expected.length}, unexpected ${unexpected.filter((f) => f.kind === 'timeout').length})  (${((Date.now() - t0) / 1000).toFixed(0)}s)`);
console.log(`functions with a crash or timeout: ${byFn.size}`);
for (const [fn, fs_] of [...byFn].sort()) {
  const tag = fn in EXPENSIVE ? ` [expensive: ${EXPENSIVE[fn]}]` : '';
  console.log(`${fn}: ` + fs_.map((f) => `${f.sc}=${f.expected ? 'expected-timeout' : f.kind}${f.kind === 'crash' ? `(${f.code ?? f.signal})` : ''}`).join(', ') + tag);
}
const outFile = opt('--json', '');
if (outFile) fs.writeFileSync(outFile, JSON.stringify(failures, null, 2));
if (unexpected.length) {
  console.log(`FAIL: ${unexpected.length} crash(es) or unexpected timeout(s)`);
}
process.exitCode = unexpected.length ? 1 : 0;
