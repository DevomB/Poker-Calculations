# Math features — contributor notes

**Roadmap IDs P1–P25 are implemented** in the native addon. Shipped API inventory: [`FEATURES_ADDED.md`](FEATURES_ADDED.md). Types and names: [`index.d.ts`](index.d.ts), [`README.md`](README.md).

Future **esoteric** ideas and quality-gated backlog: [`MATH_FEATURES_ROADMAP.md`](MATH_FEATURES_ROADMAP.md).

---

## How to add a math feature (contributor guide)

Follow the same pipeline the project already uses for chip / GTO math.

### Step 1 — Pure C++ API

1. Declare the function in [`include/poker/poker_math.hpp`](include/poker/poker_math.hpp) **or** add a focused header (e.g. `include/poker/icm.hpp`) if the feature set is large.
2. Implement in [`src/poker_math.cpp`](src/poker_math.cpp) (or matching `src/icm.cpp`, `src/side_pot.cpp`, `src/exact_equity.cpp`).
3. Match existing style from [`src/poker_math.cpp`](src/poker_math.cpp):
   - Validate inputs with `std::invalid_argument` and clear messages.
   - Use small anonymous helpers for `finite`, `non-negative`, and `clamp01` patterns.
   - Prefer `double` for chip- and probability-sized values unless an API must be integer chips.

### Step 2 — Tests (GoogleTest)

1. Add cases to [`tests/test_poker_math.cpp`](tests/test_poker_math.cpp) **or** create `tests/test_<module>.cpp` if the feature is bulky.
2. If you add a new test source file, register it in [`CMakeLists.txt`](CMakeLists.txt) under the `poker_tests` target (follow existing `test_*.cpp` entries).
3. Run `npm test` from the repo root (or `ctest` from the CMake build directory).

### Step 3 — N-API binding (Node)

1. In [`native/binding.cpp`](native/binding.cpp), add a `static Napi::Value YourFunction(...)` that parses JS arguments, calls `poker::your_function`, and returns `Napi::Number` / `Napi::Object` as appropriate.
2. Register the export in `RegisterExports` beside the other `exports.Set(Napi::String::New(env, "camelCaseName"), ...)`.
3. Rebuild: `npm run build:native`.

### Step 4 — TypeScript types

1. Update [`index.d.ts`](index.d.ts): add the method to `PokerCalculations` with JSDoc for units (chips vs BB), inclusive/exclusive pot definitions, and thrown errors.

### Step 5 — Documentation

1. Add a row to the **API overview** table in [`README.md`](README.md) when the symbol is public.
2. For esoteric features, prefer **one short paragraph** in README plus a longer “Assumptions / limitations” subsection in this file or in [`MATH_FEATURES_ROADMAP.md`](MATH_FEATURES_ROADMAP.md).
3. When a feature ships, add it to [`FEATURES_ADDED.md`](FEATURES_ADDED.md).

### Step 6 — Release discipline

- Breaking changes to names or semantics require a semver decision and changelog note in release notes (see README for prior breaking-change pattern).

---

## Implementation notes (risk register)

| Topic | Note |
| --- | --- |
| **ICM (P17–P19)** | Harville / Malmuth–Harville weighting; document elimination-order assumptions; expose payout vector explicitly. |
| **P22–P23** | Enumeration cost grows quickly; **no silent caching** inside the addon. `exactHuEquityVsRandomHand` requires board length ≥ 3. `chubukovSymmetricJamBreakevenStack` is a **closed-form toy** from equity (combine with P22 or external equity when you have it). |
| **P25** | Heuristic only — never present as exact multiway equity. |
| **P2 / P4** | Easy to get wrong with overlapping outs; prefer tests with small toy decks **or** fully specified out sets from integration tests. |

---

*Verify against `index.d.ts` and `README.md` before implementing new work.*
