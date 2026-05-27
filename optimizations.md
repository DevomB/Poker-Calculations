## Agent: Bindings & JS boundary

**Scope:** `native/binding.cpp` (~2.4k lines, sole N-API surface), `index.js` (passthrough), `index.d.ts` (types only), `src/card_string.cpp` (parse path invoked from bindings). **Stack:** node-addon-api ^8.7, N-API v8, no `Napi::AsyncWorker`; card `Uint8Array`/`Buffer` input shipped in v2.0.

### Already reasonable

| Item | Notes |
|------|--------|
| Thin JS entry | `index.js` only loads via `node-gyp-build` and re-exports native â€” no extra JS hop on hot calls. |
| C++ parallelism exists | `parallelHandSimulation` uses `std::async` in `monte_carlo.cpp`; villain/MC work is not single-threaded once past the boundary. |
| `Card` in native core | After parse, hands are 2-byte `poker::Card` values; cost is almost entirely JSâ†”C++ marshalling, not in-evaluator representation. |
| `validateCardString` | Invalid input returns `false` without throwing (good for UI validation loops). |
| Vector `reserve` | `parse_card_strings`, `doubles_from_js_array`, and `parse_game_state` reserve from array lengths. |

---

### P0 â€” Event-loop blocking & dominant marshalling tax

#### B1 â€” Synchronous heavy exports block the Node event loop

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0.0)** â€” `*Async` Promise exports via `Napi::AsyncWorker` in `native/async_workers.cpp`; sync APIs unchanged. |
| **Location** | `simulateHandOutcomeAsync`, `parallelHandSimulationAsync`, `exactHuEquityVsRandomHandAsync`, `straightMadeFlopToRiverExactProbabilityAsync`, `decideActionAsync`, `benchmarkEvaluatorThroughputAsync` |
| **Remaining** | Cancellation / `AbortSignal` (optional B1.1). |
| **~Lines** | Done |

#### B2 â€” Every card crosses the boundary as `string[]` with full UTF-8 copies

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0)** â€” `parse_cards_from_js()` accepts `string[]`, `Uint8Array`, and `Buffer`; `parse_packed_cards` in `card_string.cpp`. |
| **Location** | `parse_cards_from_js`, all card-consuming exports in `binding.cpp` |
| **Remaining** | B5 fast string path (optional); B7 packed `parseCompactCardList` output. |
| **~Lines** | Done |

#### B3 â€” `evaluateHandStrength` / `Fast` return decimal strings, not numeric strength

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0.0)** â€” `evaluateHandStrength` / `Fast` return `number`; `*Scalar` exports removed. |
| **Location** | `EvalHandStrength` helper in `binding.cpp` |

#### B4 â€” No batch export for repeated spots (JS pays full marshalling per call)

| Field | Detail |
|-------|--------|
| **Status** | **Done** — batch exports in `binding_batch.cpp`; `simulate_hand_outcome_batch` in `monte_carlo.cpp`. |
| **Location** | `native/binding_batch.cpp` |
| **Current** | One N-API transition + full card parse + return boxing per iteration. |
| **Proposal** | `simulateHandOutcomeBatch(specs: { hole, board, n, seed, villains }[])` writing into `Float64Array` out; `evaluateHandStrengthFastBatch(holes: Uint8Array, boards: Uint8Array, stride)`; optional shared `dead` mask. Single env crossing. |
| **Impact** | Very high for range builders and sim dashboards. |
| **Risk** | Medium â€” schema design and validation errors need clear indices. |
| **~Lines** | 300â€“600 |

---

### P1 â€” Reduce V8 heap churn & parsing overhead

#### B5 â€” `parse_card_string` hot path allocates and scans linearly

| Field | Detail |
|-------|--------|
| **Location** | `poker::parse_card_string` (`src/card_string.cpp` L21â€“62) |
| **Current** | Always `trim_copy` â†’ new `std::string`; rank via loop over `"23456789TJQKA"`; suit loop over `"cdhs"`. |
| **Proposal** | `parse_card_string_unchecked(const char* p, size_t n, Card& out)` for 2-char (`Ah`) / 3-char (`10h`) tokens; `constexpr` rank/suit LUT indexed by ASCII; trim only if `isspace` at ends. Binding fast path calls this on UTF-8 slice from `napi_get_value_string_utf8` with known length. |
| **Impact** | High when staying on string API; pairs with B2. |
| **Risk** | Low if old path kept for sloppy input. |
| **~Lines** | 80â€“150 |

#### B6 â€” `cardStringsHaveDuplicate` double materializes strings

| Field | Detail |
|-------|--------|
| **Location** | `strings_from_js_array` + `card_strings_have_duplicate` (`binding.cpp` L1909â€“1945; `card_string.cpp` L65â€“82) |
| **Current** | All JS strings â†’ `vector<string>` â†’ parse again â†’ `vector<Card>` â†’ O(nÂ²) pairwise compare. |
| **Proposal** | Single-pass: parse to `Card` or 6-bit id, `bool seen[52]` on stack; throw on invalid index without building string vector. |
| **Impact** | Medium for preflight validation in UIs. |
| **Risk** | Low. |
| **~Lines** | 40â€“70 |

#### B7 â€” `parseCompactCardList` allocates canonical strings twice

| Field | Detail |
|-------|--------|
| **Location** | `parse_compact_card_list` â†’ binding loop `Napi::String::New` per card (`binding.cpp` L1966â€“1977) |
| **Current** | C++ builds `vector<string>` via `Card::to_string()`; binding re-boxes each as a new JS string. |
| **Proposal** | Return `Uint8Array` of deck ids or one concatenated ASCII buffer + length; optional `outFormat: 'strings' \| 'packed'`. |
| **Impact** | Medium for import parsers feeding sims. |
| **Risk** | Low if additive. |
| **~Lines** | 60â€“120 |

#### B8 â€” Numeric matrix / ICM exports box every double

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0.0)** — `binding_numeric.cpp` Float64 in/out; ICM exports accept `returnFormat: 'float64'`. |
| **Location** | `doubles_from_js_array`, `IcmHarvillePlacementProbabilities`, `IcmExpectedPayouts`, `NormalizedStackFractions`, `LayeredPotChipEvFromEquities`, etc. |
| **Current** | In: loop `Array` â†’ `vector<double>`; Out: loop `Napi::Number::New` per element (nested arrays for matrices). |
| **Proposal** | Accept `Float64Array` in (length check + pointer); write results with `Napi::Float64Array::New(env, n, data)` or external ArrayBuffer + single memcpy from `std::vector` filled in C++. |
| **Impact** | Mediumâ€“high for large ICM grids in JS. |
| **Risk** | Low additive overload. |
| **~Lines** | 150â€“300 |

#### B9 â€” `decideAction` deep object walk with repeated `Utf8Value`

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0.0)** — PKST v1 `encodePokerState` / `decodePokerState`; `parse_state_input` in `binding_state.cpp`. |
| **Location** | `parse_game_state`, `parse_bot_config`, `parse_opponent_model` (`binding.cpp` L146â€“248+) |
| **Current** | Every player: object property gets, `holeCards` string arrays parsed; `phase` string â†’ `std::string` â†’ `unordered_map` lookup; `actedThisStreet` bool array. |
| **Proposal** | Binary state snapshot (`Uint8Array` layout: fixed header + per-player cards as bytes + ints as `Int32Array`); or cache parsed `PokerGameState` in JS as an opaque external (`Napi::External`) after first `encodeState`. |
| **Impact** | High for bots polling `decideAction` every action. |
| **Risk** | Medium â€” schema versioning. |
| **~Lines** | 200â€“400 |

#### B10 â€” `eval_to_object` builds nested object + 5-number kicker array per eval

| Field | Detail |
|-------|--------|
| **Location** | `eval_to_object`, `EvaluateBestHand` (`binding.cpp` L92â€“100, L289â€“308) |
| **Current** | `rank` string via `hand_rank_js` â†’ `std::string` â†’ JS string; kickers as `Array` of `Number`. |
| **Proposal** | Slim return: `{ rank: number, strength: number }` using enum ordinal; or return strength scalar only with `handRankCategoryOrder` in JS. Intern rank strings once at init (`Napi::String::New` stored in static array). |
| **Impact** | Medium for hand-history analyzers calling `evaluateBestHand` heavily. |
| **Risk** | Low if additive shape. |
| **~Lines** | 50â€“120 |

---

### P2 â€” Polish, export table, error-path temperature

#### B11 â€” Uniform `try` / `catch (const std::exception&)` on every export (~100+ handlers)

| Field | Detail |
|-------|--------|
| **Status** | **Partial (v2.0.0)** — `POKER_TRY` macro; batch/ICM migrated; scalars still legacy try/catch. |
| **Location** | Pattern across `binding.cpp` (e.g. L289â€“307, L403â€“432) |
| **Current** | Happy path still enters `try`; validation uses `throw std::invalid_argument` â†’ catch â†’ `Napi::Error::New` â†’ `ThrowAsJavaScriptException`. |
| **Proposal** | Split: `CHECK_ARGS` macro returns `env.Null()` after `TypeError` without exceptions; reserve exceptions for rare C++ failures. Mark hot wrappers `noexcept` where possible. Use `[[unlikely]]` on error branches. |
| **Impact** | Lowâ€“medium per call; adds up in micro-benchmarks. |
| **Risk** | Low â€” mechanical refactor. |
| **~Lines** | 200â€“400 (macro + sweep) |

#### B12 â€” `parse_card_strings` error path still throws after out-param

| Field | Detail |
|-------|--------|
| **Location** | Callers of `parse_card_strings` (e.g. `EvaluateBestHand` L296â€“298) |
| **Current** | Sets `err` string then `throw std::invalid_argument(err)` â€” extra copy and exception on common user mistakes. |
| **Proposal** | Return `std::optional<std::vector<Card>>` or `napi_status`; binding sets `TypeError` once without `std::invalid_argument` hop. |
| **Impact** | Lowâ€“medium on invalid-input benchmarks. |
| **Risk** | Low. |
| **~Lines** | 80â€“150 |

#### B13 â€” `RegisterExports` allocates ~100 property name strings at load

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0.0)** — `binding_register.cpp` with `DefineProperties` (110 exports). |
| **Location** | `RegisterExports` (`binding.cpp` L2204â€“2371) |
| **Current** | Each `exports.Set(Napi::String::New(env, "name"), Napi::Function::New(...))` creates fresh name strings and function objects. |
| **Proposal** | `Napi::ObjectReference` + `Napi::FunctionReference` statics initialized once; or `napi_define_properties` with `napi_property_descriptor` table (names as string literals, one shot). |
| **Impact** | Low (once per process). |
| **Risk** | Very low. |
| **~Lines** | 80â€“150 |

#### B14 â€” TypeScript surface offers no packed-card or async types

| Field | Detail |
|-------|--------|
| **Status** | **Done (v2.0.0 + v2.0.0)** â€” `CardInput`, `Card52` in `index.d.ts`; optional [`encode.js`](encode.js); `*Async` Promise types for heavy exports (B1). |

#### B15 â€” `hand_rank_js` returns `std::string` by value

| Field | Detail |
|-------|--------|
| **Location** | `hand_rank_js` (`binding.cpp` L84â€“89) |
| **Current** | Allocates/copy `std::string` then `Napi::String::New`. |
| **Proposal** | Return `const char*` into `kHandRankNames` + length; `Napi::String::New(env, ptr, len)`. |
| **Impact** | Low per call. |
| **Risk** | Very low. |
| **~Lines** | 15â€“25 |

#### B16 â€” `compareBestHands` parses both sides then C++ does O(nÂ²) overlap on stringsâ€™ cards

| Field | Detail |
|-------|--------|
| **Location** | `CompareBestHands` (`binding.cpp` ~L2148+) + `compare_best_hands` in `hand_evaluator.cpp` |
| **Current** | Full string parse for A and B; overlap check in C++ on `Card` (good) but only after double JS string tax. |
| **Proposal** | Packed 7+7 byte input + optional `assumeDisjoint` flag skipping overlap check when caller guarantees. |
| **Impact** | Medium in duel/compare tools. |
| **Risk** | Low additive. |
| **~Lines** | 60â€“100 |

---

### Suggested implementation order (bindings-only)

1. ~~**B3** (quick win, string â†’ number strength)~~ â€” **done in v2.0**  
2. ~~**B2 + B14** (packed cards, typed overloads)~~ â€” **done in v2.0**  
3. ~~**B1** (async for MC / exact / `decideAction`)~~ â€” **done in v2.0.0**  
4. **B4** (batch sim)  
5. **B8, B9** (TypedArray ICM / state)  
6. **B11 + B13** (cleanup)

### Top 3 impact (this section)

1. **B1** â€” Offload MC, exact enumeration, and `decideAction` from the main thread.  
2. **B2** â€” Packed `Uint8Array` card input; eliminate per-card `Utf8Value` + `trim_copy`.  
3. **B4** â€” Batch equity/sim exports to amortize N-API transition cost.
