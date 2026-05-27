## Agent: Bindings & JS boundary

**Scope:** `native/binding.cpp` (~2.4k lines, sole N-API surface), `index.js` (passthrough), `index.d.ts` (types only), `src/card_string.cpp` (parse path invoked from bindings). **Stack:** node-addon-api ^8.7, N-API v8, no `Napi::AsyncWorker`, no TypedArray paths.

### Already reasonable

| Item | Notes |
|------|--------|
| Thin JS entry | `index.js` only loads via `node-gyp-build` and re-exports native — no extra JS hop on hot calls. |
| C++ parallelism exists | `parallelHandSimulation` uses `std::async` in `monte_carlo.cpp`; villain/MC work is not single-threaded once past the boundary. |
| `Card` in native core | After parse, hands are 2-byte `poker::Card` values; cost is almost entirely JS↔C++ marshalling, not in-evaluator representation. |
| `validateCardString` | Invalid input returns `false` without throwing (good for UI validation loops). |
| Vector `reserve` | `parse_card_strings`, `doubles_from_js_array`, and `parse_game_state` reserve from array lengths. |

---

### P0 — Event-loop blocking & dominant marshalling tax

#### B1 — Synchronous heavy exports block the Node event loop

| Field | Detail |
|-------|--------|
| **Location** | `SimulateHandOutcome`, `ParallelHandSimulation`, `ExactHuEquityVsRandomHand`, `StraightMadeFlopToRiverExactProbability`, `DecideAction`, `BenchmarkEvaluatorThroughput` in `native/binding.cpp` |
| **Current** | All run to completion on the main thread; README’s “async workers” refers to C++ `std::async`, not libuv threadpool / `Napi::AsyncWorker`. Large `numSimulations` or `decideAction` MC freezes timers, I/O, and UI. |
| **Proposal** | Add `*Async` variants (or optional callback/Promise overloads) using `Napi::AsyncWorker` / `Napi::ThreadSafeFunction`: copy inputs once in `Execute()`, run C++ on worker thread, resolve with a single number/object on main thread. Keep sync APIs for scripts. |
| **Impact** | **Very high** for servers and Electron/React clients calling equity or bots. |
| **Risk** | Medium — lifetime of `Napi::Reference`, cancellation, and error propagation must be correct. |
| **~Lines** | 400–800 (worker base class + 5–6 exports) |

#### B2 — Every card crosses the boundary as `string[]` with full UTF-8 copies

| Field | Detail |
|-------|--------|
| **Location** | `parse_card_strings()` (`binding.cpp` L60–81); all equity/eval/compare/decide paths |
| **Current** | Per element: `arr[i]` → `Napi::String::Utf8Value()` → `std::string` → `parse_card_string()` → `trim_copy()` (`card_string.cpp` L22) → rank scan. Seven cards × two arrays × N calls = massive allocator + V8 string materialization traffic. |
| **Proposal** | **Primary:** accept `Uint8Array` / `Buffer` of packed cards (`0–51` deck id, or `(rank<<2)|suit` in one byte). Read via `napi_get_arraybuffer_info` / `Napi::TypedArray::Data()` with no per-card heap string. **Secondary:** overload `string[]` with a fast path: if length is 2/3 and ASCII, parse in-place from UTF-8 buffer without `trim_copy` / `std::string` owning copy. |
| **Impact** | **Very high** for any loop calling native with the same board/hole encoding. |
| **Risk** | Medium — API + `index.d.ts` breaking unless additive (`holeCards: string[] \| Uint8Array`). |
| **~Lines** | 250–450 (codec helpers + ~15 export entry points) |

#### B3 — `evaluateHandStrength` / `Fast` return decimal strings, not numeric strength

| Field | Detail |
|-------|--------|
| **Status** | **Partially done** — `evaluateHandStrengthScalar` and `evaluateHandStrengthFastScalar` return `number` via `Napi::Number::New`; legacy string exports unchanged until a future major. |
| **Location** | `EvaluateHandStrength`, `EvaluateHandStrengthFast`, `EvaluateHandStrengthScalar`, `EvaluateHandStrengthFastScalar` (`binding.cpp`) |
| **Current** | String APIs still use `std::to_string` + `Napi::String::New`; scalar APIs box once as `number` (exact for ≤28-bit encoding). |
| **Remaining** | Switch `evaluateHandStrength` / `Fast` to return `number` (breaking) or remove string boxing in a major release. |
| **Impact** | High for compare/sort pipelines — use `*Scalar` exports. |
| **Risk** | Low for additive scalar APIs; semver major only if string return types change. |
| **~Lines** | Done (~80 binding + types + docs) |

#### B4 — No batch export for repeated spots (JS pays full marshalling per call)

| Field | Detail |
|-------|--------|
| **Location** | Missing; callers loop `simulateHandOutcome` / `evaluateHandStrengthFast` / `exactHuEquityVsRandomHand` |
| **Current** | One N-API transition + full card parse + return boxing per iteration. |
| **Proposal** | `simulateHandOutcomeBatch(specs: { hole, board, n, seed, villains }[])` writing into `Float64Array` out; `evaluateHandStrengthFastBatch(holes: Uint8Array, boards: Uint8Array, stride)`; optional shared `dead` mask. Single env crossing. |
| **Impact** | Very high for range builders and sim dashboards. |
| **Risk** | Medium — schema design and validation errors need clear indices. |
| **~Lines** | 300–600 |

---

### P1 — Reduce V8 heap churn & parsing overhead

#### B5 — `parse_card_string` hot path allocates and scans linearly

| Field | Detail |
|-------|--------|
| **Location** | `poker::parse_card_string` (`src/card_string.cpp` L21–62) |
| **Current** | Always `trim_copy` → new `std::string`; rank via loop over `"23456789TJQKA"`; suit loop over `"cdhs"`. |
| **Proposal** | `parse_card_string_unchecked(const char* p, size_t n, Card& out)` for 2-char (`Ah`) / 3-char (`10h`) tokens; `constexpr` rank/suit LUT indexed by ASCII; trim only if `isspace` at ends. Binding fast path calls this on UTF-8 slice from `napi_get_value_string_utf8` with known length. |
| **Impact** | High when staying on string API; pairs with B2. |
| **Risk** | Low if old path kept for sloppy input. |
| **~Lines** | 80–150 |

#### B6 — `cardStringsHaveDuplicate` double materializes strings

| Field | Detail |
|-------|--------|
| **Location** | `strings_from_js_array` + `card_strings_have_duplicate` (`binding.cpp` L1909–1945; `card_string.cpp` L65–82) |
| **Current** | All JS strings → `vector<string>` → parse again → `vector<Card>` → O(n²) pairwise compare. |
| **Proposal** | Single-pass: parse to `Card` or 6-bit id, `bool seen[52]` on stack; throw on invalid index without building string vector. |
| **Impact** | Medium for preflight validation in UIs. |
| **Risk** | Low. |
| **~Lines** | 40–70 |

#### B7 — `parseCompactCardList` allocates canonical strings twice

| Field | Detail |
|-------|--------|
| **Location** | `parse_compact_card_list` → binding loop `Napi::String::New` per card (`binding.cpp` L1966–1977) |
| **Current** | C++ builds `vector<string>` via `Card::to_string()`; binding re-boxes each as a new JS string. |
| **Proposal** | Return `Uint8Array` of deck ids or one concatenated ASCII buffer + length; optional `outFormat: 'strings' \| 'packed'`. |
| **Impact** | Medium for import parsers feeding sims. |
| **Risk** | Low if additive. |
| **~Lines** | 60–120 |

#### B8 — Numeric matrix / ICM exports box every double

| Field | Detail |
|-------|--------|
| **Location** | `doubles_from_js_array`, `IcmHarvillePlacementProbabilities`, `IcmExpectedPayouts`, `NormalizedStackFractions`, `LayeredPotChipEvFromEquities`, etc. |
| **Current** | In: loop `Array` → `vector<double>`; Out: loop `Napi::Number::New` per element (nested arrays for matrices). |
| **Proposal** | Accept `Float64Array` in (length check + pointer); write results with `Napi::Float64Array::New(env, n, data)` or external ArrayBuffer + single memcpy from `std::vector` filled in C++. |
| **Impact** | Medium–high for large ICM grids in JS. |
| **Risk** | Low additive overload. |
| **~Lines** | 150–300 |

#### B9 — `decideAction` deep object walk with repeated `Utf8Value`

| Field | Detail |
|-------|--------|
| **Location** | `parse_game_state`, `parse_bot_config`, `parse_opponent_model` (`binding.cpp` L146–248+) |
| **Current** | Every player: object property gets, `holeCards` string arrays parsed; `phase` string → `std::string` → `unordered_map` lookup; `actedThisStreet` bool array. |
| **Proposal** | Binary state snapshot (`Uint8Array` layout: fixed header + per-player cards as bytes + ints as `Int32Array`); or cache parsed `PokerGameState` in JS as an opaque external (`Napi::External`) after first `encodeState`. |
| **Impact** | High for bots polling `decideAction` every action. |
| **Risk** | Medium — schema versioning. |
| **~Lines** | 200–400 |

#### B10 — `eval_to_object` builds nested object + 5-number kicker array per eval

| Field | Detail |
|-------|--------|
| **Location** | `eval_to_object`, `EvaluateBestHand` (`binding.cpp` L92–100, L289–308) |
| **Current** | `rank` string via `hand_rank_js` → `std::string` → JS string; kickers as `Array` of `Number`. |
| **Proposal** | Slim return: `{ rank: number, strength: number }` using enum ordinal; or return strength scalar only with `handRankCategoryOrder` in JS. Intern rank strings once at init (`Napi::String::New` stored in static array). |
| **Impact** | Medium for hand-history analyzers calling `evaluateBestHand` heavily. |
| **Risk** | Low if additive shape. |
| **~Lines** | 50–120 |

---

### P2 — Polish, export table, error-path temperature

#### B11 — Uniform `try` / `catch (const std::exception&)` on every export (~100+ handlers)

| Field | Detail |
|-------|--------|
| **Location** | Pattern across `binding.cpp` (e.g. L289–307, L403–432) |
| **Current** | Happy path still enters `try`; validation uses `throw std::invalid_argument` → catch → `Napi::Error::New` → `ThrowAsJavaScriptException`. |
| **Proposal** | Split: `CHECK_ARGS` macro returns `env.Null()` after `TypeError` without exceptions; reserve exceptions for rare C++ failures. Mark hot wrappers `noexcept` where possible. Use `[[unlikely]]` on error branches. |
| **Impact** | Low–medium per call; adds up in micro-benchmarks. |
| **Risk** | Low — mechanical refactor. |
| **~Lines** | 200–400 (macro + sweep) |

#### B12 — `parse_card_strings` error path still throws after out-param

| Field | Detail |
|-------|--------|
| **Location** | Callers of `parse_card_strings` (e.g. `EvaluateBestHand` L296–298) |
| **Current** | Sets `err` string then `throw std::invalid_argument(err)` — extra copy and exception on common user mistakes. |
| **Proposal** | Return `std::optional<std::vector<Card>>` or `napi_status`; binding sets `TypeError` once without `std::invalid_argument` hop. |
| **Impact** | Low–medium on invalid-input benchmarks. |
| **Risk** | Low. |
| **~Lines** | 80–150 |

#### B13 — `RegisterExports` allocates ~100 property name strings at load

| Field | Detail |
|-------|--------|
| **Location** | `RegisterExports` (`binding.cpp` L2204–2371) |
| **Current** | Each `exports.Set(Napi::String::New(env, "name"), Napi::Function::New(...))` creates fresh name strings and function objects. |
| **Proposal** | `Napi::ObjectReference` + `Napi::FunctionReference` statics initialized once; or `napi_define_properties` with `napi_property_descriptor` table (names as string literals, one shot). |
| **Impact** | Low (once per process). |
| **Risk** | Very low. |
| **~Lines** | 80–150 |

#### B14 — TypeScript surface offers no packed-card or async types

| Field | Detail |
|-------|--------|
| **Location** | `index.d.ts` — all card params `string[]`; no Promise overloads |
| **Current** | Types steer consumers toward highest-cost encoding; no compile-time hint for batch/async. |
| **Proposal** | Add `CardInput = string[] \| Uint8Array`, `Card52 = number` (0–51), async signatures mirroring native; optional tiny `encode.ts` (not on hot path) for string→packed if you want zero native string parsing in app code. |
| **Impact** | Ecosystem — enables B2/B4 adoption. |
| **Risk** | Very low. |
| **~Lines** | 40–100 |

#### B15 — `hand_rank_js` returns `std::string` by value

| Field | Detail |
|-------|--------|
| **Location** | `hand_rank_js` (`binding.cpp` L84–89) |
| **Current** | Allocates/copy `std::string` then `Napi::String::New`. |
| **Proposal** | Return `const char*` into `kHandRankNames` + length; `Napi::String::New(env, ptr, len)`. |
| **Impact** | Low per call. |
| **Risk** | Very low. |
| **~Lines** | 15–25 |

#### B16 — `compareBestHands` parses both sides then C++ does O(n²) overlap on strings’ cards

| Field | Detail |
|-------|--------|
| **Location** | `CompareBestHands` (`binding.cpp` ~L2148+) + `compare_best_hands` in `hand_evaluator.cpp` |
| **Current** | Full string parse for A and B; overlap check in C++ on `Card` (good) but only after double JS string tax. |
| **Proposal** | Packed 7+7 byte input + optional `assumeDisjoint` flag skipping overlap check when caller guarantees. |
| **Impact** | Medium in duel/compare tools. |
| **Risk** | Low additive. |
| **~Lines** | 60–100 |

---

### Suggested implementation order (bindings-only)

1. ~~**B3** (quick win, string → number strength)~~ — scalar exports shipped; string APIs deferred to major  
2. **B2 + B14** (packed cards, typed overloads)  
3. **B1** (async for MC / exact / `decideAction`)  
4. **B4** (batch sim)  
5. **B8, B9** (TypedArray ICM / state)  
6. **B11–B13** (cleanup)

### Top 3 impact (this section)

1. **B1** — Offload MC, exact enumeration, and `decideAction` from the main thread.  
2. **B2** — Packed `Uint8Array` card input; eliminate per-card `Utf8Value` + `trim_copy`.  
3. **B4** — Batch equity/sim exports to amortize N-API transition cost.
