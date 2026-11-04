<h1 align="center">Poker-Calculations</h1>

<p align="center">
  Poker math and simulation for Node.js, with a C++20 core and prebuilt N-API binaries.
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/DevomB/Poker-Calculations/main/Poker-Calculations-Image.png" alt="Poker-Calculations" width="720" />
</p>

<p align="center">
  <a href="https://www.npmjs.com/package/poker-calculations">npm</a> ·
  <a href="https://poker-calculations.devomb.com">Documentation</a> ·
  <a href="https://github.com/DevomB/Poker-Calculations">GitHub</a>
</p>

## What it does

- **Texas Hold'em** (unprefixed functions): best-hand evaluation; Monte Carlo equity (sync, async, parallel, batch, with uncertainty, and against one weighted range per opponent); exact heads-up and multiway equity; range tools over the 1326 combos and 169 hand classes; hand potential (HS, PPot, NPot, EHS2); suit-isomorphic boards.
- **Chip and tournament math:** pot odds, call and raise EV with rake, SPR, MDF, fold-equity breakevens, Harrington M, Kelly; Harville ICM, bubble factors, side-pot ladders, PKO bounties, Nash push/fold, and small river and subgame solvers.
- **Other games** (prefixed functions): Omaha Hi and Hi-Lo, Big O, short deck (6+), seven-card stud and razz, 2-7 single draw.
- **Table state:** a PKST binary state format, validation, legal actions, and `decideAction`, a rule-based policy from Monte Carlo equity and pot odds for simulators and research.

What kind of answer each family gives:

- **Exact:** hand evaluation, `exact*` equity and runout enumerations, combinatorics, chip-math formulas, side-pot ladders, and Harville ICM (computed over subsets of players: every place for up to 20 players, fewer places for larger fields).
- **Monte Carlo estimates:** `simulate*` equity (seeded, with standard errors where reported) and the preflop and push/fold equity tables.
- **Numerical approximations:** the ICM stack Jacobian (central differences), iterative solvers (river CFR, fictitious play, Nash push/fold by fictitious play over 169 hand classes with one representative combo per class).
- **Heuristics:** `decideAction` and the opponent-model helpers.
- **Simplified tournament models:** PKO bounty collection by stack ratios, the FGS blind-decay model, and the deprecated power-utility allocator `icmExpectedPayoutsWeitzman`, which is not a finishing-position model.

It is a calculation library. It has no full-game solver, no HTTP layer, and the state and decision APIs model no-limit Hold'em only.

## Install

Requires Node.js 18 or later.

```bash
npm install poker-calculations
```

Releases ship prebuilt binaries for Linux (x64, arm64, and armv7 on glibc; x64 and arm64 on musl), macOS (arm64, x64), and Windows (x64, arm64), so installing needs no compiler. Linux x64 and armv7 glibc binaries are built on Debian bullseye with static libstdc++. Other platforms build from source (see [Development](#development)).

## Usage

```js
const poker = require('poker-calculations'); // or: import poker from 'poker-calculations';

const hand = poker.evaluateBestHand(['Ah', 'Kh', 'Qh', 'Jh', 'Th', '2c', '3d']);
console.log(hand.rank); // royalFlush

// Equity of AhKh on Qh Jh 2c against one random hand: 20,000 seeded trials.
const equity = poker.simulateHandOutcome(['Ah', 'Kh'], ['Qh', 'Jh', '2c'], 20_000, 42, 1);

// Villain bets 30 into 60. `pot` counts villain's bet, so it is 90 when hero acts.
console.log(poker.breakevenCallEquity(90, 30)); // 0.25
console.log(poker.expectedValueCall(equity, 90, 30) > 0); // true
```

Every `*Async` export runs on the libuv thread pool and accepts a final `{ signal }` (`AbortSignal`); aborting rejects with `AbortError`. `poker-calculations/encode` converts cards to and from deck ids and wraps the PKST state codec.

## Conventions

- **Cards** are strings such as `'Ah'` or `'td'` (case-insensitive; `'10h'` also parses; anything else, such as `'Ahx'`, is rejected), or a `Uint8Array` of deck ids `rank * 4 + suit` (rank 0 = 2 … 12 = A; suits c, d, h, s).
- **Equity** is hero's expected share of the pot at showdown; a k-way tie pays `1/k`.
- **Pot** (`pot`, `potBeforeCall`) is everything in the middle when hero acts, villain's bet included: `expectedValueCall(e, pot, toCall) = e × pot − (1 − e) × toCall`.
- **Monte Carlo** results are reproducible for a given seed on one platform; low digits can differ between platforms.
- **Arrays** over hands use fixed orders: 1326 combos lexicographic by deck id, and 169 classes `22, 32s, 32o, …, AKo, AA`.
- **Errors:** invalid input throws a JavaScript error rather than terminating the process (`npm run sweep` checks every export).

Formulas and modelling assumptions are in [NUMERICAL.md](NUMERICAL.md).

## API reference

[`index.d.ts`](index.d.ts) is the authoritative list of exports, signatures, and per-function notes. The [documentation site](https://poker-calculations.devomb.com/docs/reference/api) groups them by topic with runnable examples and guides.

In bundled or serverless builds, `require` the package from runtime code (for example inside a request handler) so the bundler does not evaluate the native addon at build time, and deploy a binary that matches the target OS and libc.

## Development

Building from source needs CMake 3.16+ and a C++20 toolchain (GCC, Clang/Xcode, or MSVC).

```bash
npm ci
npm run build:native
node scripts/stage-prebuild.js linux-x64   # or win32-x64, darwin-arm64, ...; add `musl` on Alpine
npm run smoke
npm test
```

| Command | Checks |
| --- | --- |
| `npm test` | Unit tests and every `scripts/verify-*.mjs` check |
| `npm run test:types` | `index.d.ts` compiles for a strict TypeScript consumer |
| `npm run test:native` | C++ `GameEngine` betting rules and index helpers (CMake + CTest) |
| `npm run sweep` | Calls every export with edge-case inputs in child processes; fails on a crash |
| `npm run smoke` | Loads the staged binary through `index.js` (main thread and workers) |
| `npm run verify-lock` | `package-lock.json` matches `package.json`; rewrites it locally, fails under `CI=true` |

Binaries live at `prebuilds/<platform>-<arch>/node.napi.node` (plus `node.napi.musl.node` for musl) and are not committed. `node-gyp-build` prefers a local `build/Release` addon; set `PREBUILDS_ONLY=1` to load the staged prebuild the way an installed package does.

C++ consumers can link the static `poker_lib` target (`cmake -S . -B build && cmake --build build`) with headers in `include/poker/`. It also contains APIs that are not exported to Node: `GameEngine` (dealing and no-limit betting over `PokerGameState`), `Deck`, and `BotConfig` file load/save.

## Releases

Merging a version bump to `main` runs [`npm-publish.yml`](.github/workflows/npm-publish.yml): the test gate above, a build and smoke test of every platform binary, a check that the packed tarball installs and loads, then publishing with [npm trusted publishing](https://docs.npmjs.com/trusted-publishers) and tagging the published commit `v<version>`. If a run fails before publishing, fix it and re-run without bumping again. Running the workflow manually for a version already on npm only adds a missing tag.

To publish by hand, stage binaries under `prebuilds/` and run `npm publish`; `prepack` refuses to pack without at least one binary (`SKIP_PREBUILD_CHECK=1` overrides).

Release notes are in [CHANGELOG.md](CHANGELOG.md); report vulnerabilities as described in [SECURITY.md](SECURITY.md).

## Responsible use

Use it for simulators, research, and automation you are permitted to run. It is not meant for bypassing the terms of real-money sites.

## License

[ISC](LICENSE)
