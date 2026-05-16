# poker-calculations — NL Hold’em engine & odds helpers (C++ core, Node addon)

**No-Limit Texas Hold’em** hand evaluation, **Monte Carlo equity** (single- and multi-threaded), pot-odds and chip-EV helpers, and a rule-based **`decideAction`** layer—implemented in **C++** and exposed to Node via **N-API**.

Published releases ship **prebuilt native binaries** ([`node-gyp-build`](https://github.com/prebuild/node-gyp-build)), so **`npm install` does not require CMake, a compiler, or the Windows SDK** on the installing machine.

Full TypeScript types: **[`index.d.ts`](index.d.ts)**.

## Install

**Requirements:** Node.js 18+.

```bash
npm install poker-calculations
```

Published tarballs include **N-API** prebuilds under `prebuilds/<platform>-<arch>/`:

- **`node.napi.node`** — glibc Linux, macOS, Windows (default).
- **`node.napi.musl.node`** — same directory on **linux-x64** / **linux-arm64** when you install on **Alpine** / musl.

Linux **glibc** binaries are linked with **`-static-libstdc++`** / **`-static-libgcc`** so they do not require as new a system **`libstdc++.so`** as a binary built on the latest Ubuntu runner (this avoids **`GLIBCXX_*`** version errors on older Linux images, including many **serverless** hosts).

### Bundlers and Next.js

Load this package from **runtime** code paths (for example a lazy **`require()`** inside a route handler) if your bundler or **`next build`** evaluates server modules statically. That avoids optional build-time native resolution issues; you still need a **prebuild that matches** the deployment OS and libc (glibc vs musl).

## Quick start

This is a **Node.js library**—`require` or `import` it and call functions. There are **no HTTP endpoints** unless you wrap it yourself.

### CommonJS

```js
const poker = require('poker-calculations');

poker.evaluateBestHand(['Ah', 'Ac', 'Kd', 'Ks', 'Qh']); // native — best 5 of 7
const equity = poker.simulateHandOutcome(['Ah', 'Kh'], ['Qh', 'Jh', 'Th'], 2000, 42, 1);
const spr = poker.spr(90, 270); // native (C++ chip math)
```

### ESM

```js
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const poker = require('poker-calculations');
```

Runnable sample: **[`examples/demo.mjs`](examples/demo.mjs)** (from repo root: `node examples/demo.mjs`).

Cards are strings like `"Ah"`, `"Td"` (ten may be `"10h"`).

## API overview

All symbols below are exported from the **native addon** (C++ via N-API). **`breakevenCallEquity(potBeforeCall, toCall)`** equals **`potOddsRatio(pot, toCall)`** when the same pot and call amounts are used.

| Area | Exports |
| --- | --- |
| **Hands & equity** | `evaluateBestHand`, `evaluateHandStrength`, `evaluateHandCategory`, `simulateHandOutcome`, `parallelHandSimulation` |
| **Strategy** | `decideAction` |
| **Pot / EV** | `potOddsRatio`, `expectedValueCall`, `breakevenCallEquity` |
| **Stacks & display** | `spr`, `effectiveStack`, `stackInBigBlinds`, `potOddsRatioDisplay`, `formatPotOdds` |
| **Heuristics** | `ruleOfFourEquity`, `ruleOfTwoEquity`, `impliedBreakevenFutureWin` |
| **GTO-style** | `minimumDefenseFrequency`, `alphaFrequency`, `bluffToValueRatio`, `valueToBluffRatio` |
| **Sizing & commitment** | `betAsPotFraction`, `sprAfterCall`, `commitmentRatio` |
| **Fold equity** | `breakevenFoldEquityPureBluff`, `breakevenFoldEquitySemiBluff` |

**Breaking change (v1.2.0):** `poker-math.js` was removed; require `poker-calculations` (or the `.node` binding) for all math. Rebuild native artifacts after upgrading from a git clone.

## Responsible use

Use this for **your own simulator, research, or automation you are permitted to run**. It is not intended to help bypass third-party terms of service on real-money sites.

## Features (engine)

| Area | What’s included |
| --- | --- |
| **Cards / deck** | 52-card deck, shuffle with injected `std::mt19937`, deal, burn on board deals in `GameEngine` |
| **State & rules** | `PokerGameState`, blinds, pot, per-street commits, phase machine (pre-flop → river → showdown), `GameEngine::apply_action` with `Decision` |
| **Evaluation** | Best five of up to seven cards, full ranking + kickers, `evaluate_hand_strength` scalar |
| **Strategy** | `decide_action(..., BotConfig, OpponentModel*)` using MC equity (or strength fallback when sim count is 0), pot odds, and call EV |
| **Simulation** | `simulate_hand_outcome`, `parallel_hand_simulation` (chunked async workers, distinct seeds) |
| **Config** | `BotConfig::load_from_config_file` / `save_to_config_file` (`key=value`, `#` comments) |
| **Tests** | GoogleTest suite (deck, engine, evaluator, poker math, strategy, opponent model, MC, config) |

## Developing from source

If you **clone** the repo or install from a **git URL** without local prebuilds, you need a **C++ toolchain** (CMake 3.16+, and MSVC with C++ workload on Windows, Xcode CLI tools on macOS, or GCC on Linux). Published tarballs from npm do not compile native code during install.

```bash
npm ci
npm run build:native
node scripts/stage-prebuild.js <platform-arch>
```

Use the `<platform-arch>` tuple [`node-gyp-build` expects](https://github.com/prebuild/node-gyp-build) (for example `win32-x64`, `linux-x64`, `darwin-arm64`). For **Alpine/musl**, stage with `node scripts/stage-prebuild.js linux-x64 musl` (writes `node.napi.musl.node`). On Windows, delete a stale `build` folder if configure fails; ensure the **Windows SDK** is installed if you see resource-compiler (`rc`) or manifest (`mt`) errors.

Rebuild after changing C++:

```bash
npm run build:native
```

### Native tests

```bash
npm test
```

On Windows this expects MSVC on `PATH` like the build steps above.

## Maintainers: publishing

Publishing uses [`.github/workflows/npm-publish.yml`](.github/workflows/npm-publish.yml) and runs **only when you start it** (Actions → **npm publish** → **Run workflow**). Pushes to `main` do **not** automatically publish, so a failed native matrix does not “consume” the next semver before npm sees it.

### npm Trusted Publishing (OIDC)

Publishing uses **[trusted publishing](https://docs.npmjs.com/trusted-publishers)** — GitHub Actions proves identity to npm with **OIDC**; you **do not** store an **`NPM_TOKEN`** secret for `npm publish`.

1. On [npmjs.com](https://www.npmjs.com/) → package **`poker-calculations`** → **Settings** → **Trusted Publisher**, connect **GitHub Actions** using:
   - Repository that matches **`repository.url`** in [`package.json`](package.json) **exactly** (npm validates at publish time; npm does not validate when you save). Current value: `git+https://github.com/DevomB/Poker-Calculations.git`
   - Workflow filename **`npm-publish.yml`** (same casing and `.yml` extension). If you previously used **`native-prebuild.yml`**, update the trusted publisher entry on npm to this filename (or add a second allowed workflow and remove the old one).
2. After proving publishes work, optionally tighten **Publishing access** (“Require 2FA and disallow tokens”) and revoke old automation tokens, per npm’s migration guidance.

All dependencies used during CI are public; **`npm ci`** does not need a read token. If you later add **private** npm dependencies, use a **read-only** granular token only on install steps, not for publish.

### Release steps

1. On **`main`**, bump **`package.json`** `version` and keep **`package-lock.json`** in sync (for example `npm install --package-lock-only` after dependency or version changes), then push.
2. Open **Actions** → **npm publish** → **Run workflow** (branch **`main`**).

The release gate runs **`npm ci`** on `main` (so a broken or stale lockfile fails fast), then checks whether **`name@version` already exists** on npm. If not, it builds native targets (including **musl** artifacts as `node.napi.musl.node`), merges them under `prebuilds/`, and runs **`npm publish`** via OIDC. **No git tags** — the version field is the release input. With trusted publishing on a **public** repo, npm records **provenance** automatically. If that version is already on npm, the workflow skips build and publish.

**If publish fails:** fix the underlying issue, push commits **without** bumping `version` again, and **re-run the same workflow** until it succeeds — then increment only for the *next* release. That keeps npm version numbers from skipping.

Use **GitHub-hosted** runners for this workflow: OIDC trusted publishing does not support **self-hosted** runners yet ([npm docs](https://docs.npmjs.com/trusted-publishers)). The workflow pins **Node ≥22.14** to satisfy npm’s trusted-publishing runtime requirement alongside **npm CLI ≥11.5.1** in the publish job.

In the GitHub repo, under **Settings → Actions → General → Workflow permissions**, use the default that allows Actions to run; the **publish** job sets **`id-token: write`** so OIDC works for `npm publish`.

**Manual publish:** assemble binaries under `prebuilds/`, then `npm publish`. Without binaries, `prepack` fails unless `SKIP_PREBUILD_CHECK=1`.

<details>
<summary>Plain CMake (library / C++ consumers)</summary>

### Repository layout

```text
include/poker/     Public headers (Card, Deck, GameEngine, HandEvaluator, poker_math, …)
src/               Implementations (`poker_math.cpp` — SPR, MDF, fold equity, …)
native/            Node-API binding (built when CMAKE_JS_INC is set by cmake-js)
tests/             Unit tests
examples/          demo.mjs (Node)
CMakeLists.txt     Static poker_lib; optional poker_tests; optional poker_calculations.node when built by cmake-js
```

### Generic (single-configuration generators)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run tests:

```bash
cd build
ctest --output-on-failure
```

### Windows: MSVC with NMake (when `cl` is not on PATH)

Open **x64 Native Tools** or run `vcvars64.bat`, then:

```bat
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -S . -B build_nmake
cmake --build build_nmake
cd build_nmake
ctest --output-on-failure
```

### CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `POKER_BUILD_TESTS` | `ON` (non–cmake-js); `OFF` if `CMAKE_JS_INC` is set | Build `poker_tests` and GoogleTest. Off for fast npm addon builds. |

When **cmake-js** configures the project, it defines `CMAKE_JS_INC` and only **`poker_calculations.node`** plus **`poker_lib`** are built—tests are skipped so the addon build stays fast and does not pull GoogleTest.

Disable tests manually:

```bash
cmake -S . -B build -DPOKER_BUILD_TESTS=OFF
```

### GCC note

Tests link `libstdc++fs` when using GNU C++ for `std::filesystem` in config tests.

</details>

## Configuration file (`BotConfig`)

```ini
# bot.txt
aggression_threshold=0.55
risk_tolerance=0.92
monte_carlo_simulations=800
monte_carlo_villains=1
raise_pot_fraction=0.55
opponent_aggression_weight=0.05
rng_seed=2463534242
```

Load with `BotConfig::load_from_config_file("bot.txt")`.

## Quick C++ API sketch

- **State & engine**: `poker::PokerGameState`, `poker::GameEngine::start_new_hand`, `apply_action`, `advance_phase_if_ready`
- **Hands**: `poker::evaluate_best_hand`, `poker::evaluate_hand_strength`, `poker::evaluate_hand`
- **Equity**: `poker::simulate_hand_outcome`, `poker::parallel_hand_simulation`
- **Decision**: `poker::decide_action(state, hero_hole_cards, cfg, opponent_model, hero_seat)`
- **Chip / GTO math**: `poker::spr`, `poker::minimum_defense_frequency`, `poker::breakeven_fold_equity_pure_bluff`, … (`poker_math.hpp`)
- **Integration**: subclass `poker::PokerBotInterface` or use `poker::MockPokerBotInterface` for tests

Headers live under `include/poker/`. Link against **`poker_lib`**.

## Contributing

Strategy thresholds and MC counts are centralized in **`BotConfig`**. After changes, run **`npm test`** or **`ctest`**; MC-heavy tests use statistical bands (e.g. AA pre-flop equity vs one random hand).

## License

[ISC](LICENSE) — see [`LICENSE`](LICENSE) in this repository.
