// Crash sweep: call every export under every scenario in args.mjs, each batch in a child process,
// and report calls that kill the process (native crash or abort) or run past --timeout.
// A thrown JS error is the expected outcome for bad input and counts as a pass.
//
// Usage: npm run sweep -- [--shards 8] [--timeout 15000] [--only name,name] [--json out.json]
// Timeouts are informational: exact preflop enumeration and the first Nash call are slow by design.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { parseSignatures } from './signatures.mjs';
import { SCENARIOS, argsFor } from './args.mjs';

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
console.log(`calls ${jobs.length}  ok ${tally.ok}  threw ${tally.threw}  crashes ${failures.filter((f) => f.kind === 'crash').length}  timeouts ${failures.filter((f) => f.kind === 'timeout').length}  (${((Date.now() - t0) / 1000).toFixed(0)}s)`);
console.log(`functions with a crash or timeout: ${byFn.size}`);
for (const [fn, fs_] of [...byFn].sort()) {
  console.log(`${fn}: ` + fs_.map((f) => `${f.sc}=${f.kind}${f.kind === 'crash' ? `(${f.code ?? f.signal})` : ''}`).join(', '));
}
const outFile = opt('--json', '');
if (outFile) fs.writeFileSync(outFile, JSON.stringify(failures, null, 2));
process.exitCode = failures.some((f) => f.kind === 'crash') ? 1 : 0;
