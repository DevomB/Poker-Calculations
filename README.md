<h1 align="center">Poker-Calculations</h1>

<p align="center">
  <strong>No-limit Hold’em math and simulation for Node.js</strong><br />
  C++20 core · N-API prebuilds · full TypeScript types
</p>

<p align="center">
  <img src="Poker-Calculations-Image.png" alt="Poker-Calculations — poker math solved" width="720" />
</p>

<p align="center">
  <a href="https://www.npmjs.com/package/poker-calculations">NPM</a> ·
  <a href="https://poker-calculations.devomb.com">Documentation</a> ·
  <a href="https://github.com/DevomB/Poker-Calculations">GitHub</a>
</p>

---

**Poker-Calculations** is a production-ready Node library for NL Hold’em: fast hand evaluation, Monte Carlo and exact equity, pot odds and chip EV, ICM and side pots, draw probabilities, GTO-style frequencies, fold-equity models, Kelly and jam analysis, and a rule-based **`decideAction`** layer over serialized table state. Everything runs in native code and ships with **prebuilt binaries**—`npm install` does not require CMake, a compiler, or the Windows SDK.

## What you can build

- **Equity calculators** and training tools with reproducible Monte Carlo seeds
- **Simulators and bots** tuned via `BotConfig` and optional opponent models
- **Tournament tools** with Harville ICM, bubble factors, and layered side-pot EV
- **Analysis backends** (wrap the library yourself—there are no built-in HTTP endpoints)

## Highlights

| | |
| --- | --- |
| **Hands & equity** | Best-five evaluation, parallel MC simulation, exact HU equity vs a random hand, draw and runner-runner probabilities |
| **Table math** | SPR, pot odds, rake-aware call EV, breakeven equity, Harrington *M* / *Q*, sizing and commitment |
| **Strategy** | `decideAction` from serialized state using MC equity, pot odds, and call EV |
| **Tournaments** | ICM (Harville and Weitzman chip utility), placement and payout expectations, pairwise bubble factor, side-pot ladders |
| **Theory helpers** | MDF / alpha, fold-equity breakevens, Kelly and Chubukov symmetric-jam search, Wilson and Agresti–Coull intervals, risk-of-ruin approximations |
| **Tournament & runouts** | Shapley ICM, runout equity spread, range materialization, subgame bet toys — [guide](https://poker-calculations.devomb.com/docs/guides/tournament-runouts-and-bots) |
| **Developer experience** | **[`index.d.ts`](index.d.ts)** typings, **140** native exports, docs at [poker-calculations.devomb.com](https://poker-calculations.devomb.com) |

Published releases include **N-API prebuilds** for Linux (glibc and musl), macOS, and Windows via [`node-gyp-build`](https://github.com/prebuild/node-gyp-build). Linux glibc builds use static libstdc++/libgcc where needed so older server and serverless images avoid `GLIBCXX_*` mismatches.

## Install

**Node.js 18+**

```bash
npm install poker-calculations
```

## Quick start

Cards use strings like `"Ah"` and `"Td"` (ten may be `"10h"`), or packed **`Uint8Array`** deck ids (`0..51`) for hot paths — see [`encode.js`](encode.js) and [Packed card input](https://poker-calculations.devomb.com/docs/concepts/packed-card-input) on the docs site.

### CommonJS

```js
const poker = require('poker-calculations');

poker.evaluateBestHand(['Ah', 'Ac', 'Kd', 'Ks', 'Qh', 'Jh', 'Th']);
// → { rank, rankCategory, strength, kickers }

poker.parseCompactCardList('AhKhQh', { outFormat: 'packed' }); // Uint8Array for hot paths
poker.evaluateBestHand(['Ah', 'Ac', 'Kd', 'Ks', 'Qh'], { format: 'slim' });
// → { rankCategory, strength } only

const equity = poker.simulateHandOutcome(
  ['Ah', 'Kh'],
  ['Qh', 'Jh', 'Th'],
  10_000,
  42,
  1
);

// Non-blocking (libuv thread pool) — same args as sync; returns a Promise
const equityAsync = await poker.simulateHandOutcomeAsync(
  ['Ah', 'Kh'],
  ['Qh', 'Jh', 'Th'],
  10_000,
  42,
  1
);

// Optional AbortSignal (cooperative cancel; rejects with AbortError)
const ac = new AbortController();
const equityCancellable = await poker.simulateHandOutcomeAsync(
  ['Ah', 'Kh'],
  ['Qh', 'Jh', 'Th'],
  10_000,
  42,
  1,
  { signal: ac.signal }
);

const spr = poker.spr(90, 270);
const mdf = poker.minimumDefenseFrequency(100, 50);
```

### ESM

```js
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const poker = require('poker-calculations');
```

Walkthroughs, guides, and the full API live on the docs site: [introduction](https://poker-calculations.devomb.com/docs/intro) · [API reference](https://poker-calculations.devomb.com/docs/reference/api).

### Optional subpath: `poker-calculations/encode`

Six pure-JS helpers for deck ids and PKST bytes (no extra native exports):

```js
const encode = require('poker-calculations/encode');
encode.packCards(['Ah', 'Kh']);
encode.packPokerState(nativePokerState);
```

See [Packed card input](https://poker-calculations.devomb.com/docs/concepts/packed-card-input) and [Packed poker state](https://poker-calculations.devomb.com/docs/concepts/packed-poker-state) on the docs site.

## API at a glance

All exports come from the native addon. Grouped overview—see the [reference](https://poker-calculations.devomb.com/docs/reference/api) for signatures and examples.

| Area | Examples |
| --- | --- |
| **Hands & equity** | `evaluateBestHand`, `evaluateHandStrength`, `evaluateHandStrengthFast`, `simulateHandOutcome`, `simulateHandOutcomeAsync`, `parallelHandSimulation`, `exactHuEquityVsRandomHand`, … |
| **Strategy** | `decideAction`, `decideActionAsync` |
| **Pot / EV** | `potOddsRatio`, `expectedValueCall`, `breakevenCallEquity`, `rakeFromPot` |
| **Stacks & display** | `spr`, `harringtonM`, `harringtonQ`, `stackInBigBlinds`, `formatPotOdds` |
| **Draws & heuristics** | `ruleOfTwoEquity`, `hypergeometricOneCardHitProbability`, `flopToRiverAtLeastOneHitProbability` |
| **GTO-style** | `minimumDefenseFrequency`, `alphaFrequency`, `bluffToValueRatio` |
| **Fold equity** | `breakevenFoldEquityPureBluff`, `breakevenFoldEquitySemiBluff` |
| **ICM & side pots** | `icmExpectedPayouts`, `icmExpectedPayoutsWeitzman`, `icmPairwiseBubbleFactor`, `sidePotLadderFromCommitments` |
| **Stats & risk** | `wilsonScoreInterval`, `riskOfRuinDiffusionApprox`, `monteCarloStandardError` |
| **Kelly & jam** | `kellyCriterionBinary`, `chubukovSymmetricJamEv`, `chubukovMaxSymmetricJamStackBinarySearch` |

A complete inventory is in [`FEATURES_ADDED.md`](FEATURES_ADDED.md).

## Bundlers and serverless

Load from **runtime** code paths (for example a lazy `require()` inside a route handler) if your bundler or `next build` evaluates server modules at build time. You still need a **prebuild that matches** deployment OS and libc (glibc vs musl on Linux).

## Responsible use

Use this for simulators, research, and automation you are permitted to run. It is not intended to bypass third-party terms of service on real-money sites.

## License

[ISC](LICENSE)

---

<details>
<summary><strong>Developing from source</strong></summary>

Clone installs without local prebuilds need CMake 3.16+ and a C++ toolchain (MSVC on Windows, Xcode CLI on macOS, GCC on Linux).

```bash
npm ci
npm run build:native
node scripts/stage-prebuild.js <platform-arch>
```

Use tuples like `win32-x64`, `linux-x64`, `darwin-arm64`. For Alpine/musl: `node scripts/stage-prebuild.js linux-x64 musl`.

</details>

<details>
<summary><strong>Maintainers — publishing</strong></summary>

Publishing is automated via [`.github/workflows/npm-publish.yml`](.github/workflows/npm-publish.yml) on `main` when `package.json` / `package-lock.json` change, using [npm trusted publishing (OIDC)](https://docs.npmjs.com/trusted-publishers). Bump `version` on `main`, keep the lockfile in sync, and let CI build prebuilds and publish when that version is not already on npm.

Trusted publisher settings on npm must match `repository.url` in [`package.json`](package.json) and workflow filename `npm-publish.yml`. Manual publish: stage binaries under `prebuilds/`, then `npm publish` (or set `SKIP_PREBUILD_CHECK=1` only when intentionally publishing without binaries).

</details>

<details>
<summary><strong>C++ consumers (CMake)</strong></summary>

Headers under `include/poker/`. Build the static `poker_lib`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

When built via cmake-js for Node, `poker_calculations.node` and `poker_lib` are produced.

**Sketch:** `PokerGameState`, `GameEngine::apply_action`, `evaluate_best_hand`, `simulate_hand_outcome`, `decide_action`, and chip math in `poker_math.hpp`.

</details>
