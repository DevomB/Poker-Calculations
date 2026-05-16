# Math features — planned work and contributor notes

This document lists **twenty-five planned** math additions (deeper combinatorics, tournament theory, and statistics), **how to add** a new feature in this repository, and **implementation risk** notes for selected items.

Authoritative public API for shipped symbols: [`index.d.ts`](index.d.ts) and [`README.md`](README.md). Shipped inventory: [`FEATURES_ADDED.md`](FEATURES_ADDED.md).

---

## Planned features (25)

Each row: **ID**, **name**, **definition**, **references** (standard poker / probability literature — consult for formulas, not for API design), **suggested inputs → outputs**, **difficulty** (S = small closed form, M = moderate code + tests, L = new module + careful API contracts), **depends on** (other IDs or “—”).

| ID | Name | Definition | References | Inputs → outputs | Diff. | Deps |
| --- | --- | --- | --- | --- | --- | --- |
| P1 | Exact single-street hypergeometric hit probability | P(at least one “out” on the next board card) given **known** number of unseen cards and clean out count (dead cards implicit in deck size). | Standard hypergeometric sampling; complements `ruleOfTwoEquity`. | `outs`, `unseen_cards` → probability in `[0,1]` | S | — |
| P2 | Flop-to-river “at least one hit” with structured out set | For a defined out set, P(hit by river) using two draws without treating streets as independent Bernoulli trials when “at least one” is the event. | Enumerative probability texts; poker combo counting. | Out multiset or disjoint categories, `unseen_after_flop` → probability | M | P1 (optional shared helpers) |
| P3 | Runner-runner backdoor flush (exact) | Two-street probability of making a backdoor flush from a defined flush-draw state. | General probability on a 52-card deck. | Known suits in hero/board → probability | S | — |
| P4 | Runner-runner straight (structured enumeration) | For specific gap / double-belly patterns, enumerate remaining rank paths rather than multiplying independent street probabilities. | Combo counting; careful with wraparound and board pairing. | Pattern descriptor + dead cards → probability | M | — |
| P5 | Multiway symmetric breakeven call equity | Hero calls facing additional **k** callers each putting the same amount; pot grows linearly; specify showdown chip-split model (e.g. winner-take-all among tied best hands). | Multiway pot odds discussions; *Theory of Poker*-style extensions. | `pot_before`, `to_call`, `k`, model flags → breakeven equity | M | — |
| P6 | Reverse implied odds ceiling | Maximum **additional** future chips hero can lose (beyond current call) while keeping the **current** call at chip-EV ≥ 0 for stated equity — dual bookkeeping to `impliedBreakevenFutureWin`. | Sklansky-style implied / reverse implied odds. | `pot_before_call`, `to_call`, `equity` → max future loss | S | — |
| P7 | Geometric multi-street pot (HU, matched sizing) | Closed form for pot size after **n** streets where both players keep matching a constant **pot fraction** bet sequence. | Toy game tree algebra; Chen & Ankenman–style abstractions. | `pot0`, fraction `f`, `n` → pot after line | S | — |
| P8 | Two-street cascade pure-bluff fold equity | Required fold equity on street 1 and/or street 2 for a **pure air** two-barrel line with fixed sizings (extends `breakevenFoldEquityPureBluff`). | Multi-street bluffing EV algebra. | Pot line params → `FE1`, `FE2`, or compound condition | M | P7 (optional) |
| P9 | Rake-adjusted breakeven call equity | Breakeven **true** equity when won pots pay rake under a stated model (e.g. % of pot, cap). | Live/online rake structures; adjust pot won, not “pot odds” alone. | `pot_before_call`, `to_call`, rake params → equity | M | — |
| P10 | Rake-adjusted semi-bluff FE breakeven | Same rake model inside the semi-bluff fold-equation used by `breakevenFoldEquitySemiBluff`. | Combines P9 with existing semi-bluff formula. | Same as today + rake params → FE | M | P9 |
| P11 | Harrington **M** and effective **M** | `M = stack / (sb + bb + total_antes)` and variants (e.g. antes only from active players). | Harrington tournament volumes. | Stacks, blinds, antes, active players → M | S | — |
| P12 | Kelly fraction (binary outcome) | Optimal log-utility bet fraction for a one-shot gamble: known win probability and odds. | Kelly (1956); standard gambling math texts. | `p`, net odds `b` (win `b` per unit risked) → `f*` | S | — |
| P13 | Risk of ruin (diffusion / normal approx.) | `ROR ≈ exp(-2 μ B / σ²)` style estimate for cash play under Gaussian per-hand profit assumptions. | Gambling risk texts; document assumptions clearly. | `μ`, `σ`, bankroll `B` → ROR | S | — |
| P14 | Bankroll for target ROR | Inverse of P13: solve `B` given target ROR, μ, σ. | Same as P13. | `μ`, `σ`, target ROR → `B` | S | P13 |
| P15 | Monte Carlo standard error | For binomial proportion estimate `p̂` from `n` trials: `SE = sqrt(p̂(1-p̂)/n)` with conventions at 0/1. | Any intro mathematical statistics text. | `p_hat`, `n` → SE | S | — |
| P16 | Wilson score interval for MC equity | Lower/upper confidence limits for a binomial proportion; better than normal approx for small `n`. | Wilson (1927); Agresti–Coull/Wilson in modern stats references. | `successes`, `n`, confidence → `(lo, hi)` | M | P15 (optional) |
| P17 | ICM elimination probabilities (Harville / iterative) | Finish-order probabilities from stacks alone under Malmuth–Harville weighting (chip-proportional elimination). | Tournament ICM literature; Malmuth; SNG software docs. | `stacks[]` → elimination / placement distribution | L | — |
| P18 | ICM $EV per player | Expected **payout** under P17 given a payout vector (e.g. final table prizes). | Same as P17. | `stacks[]`, `payouts[]` → `$EV` per seat | L | P17 |
| P19 | ICM bubble factor (pairwise) | Ratio of marginal **$** loss from losing a pot vs marginal **$** gain from winning it between two players, via finite differences on P18. | “Bubble factor” in MTT modeling. | `stacks[]`, `payouts[]`, `(i,j)`, pot chips → factor | L | P18 |
| P20 | Side-pot ladder from commitments | From sorted all-in commitment amounts, build main + side pot **chip partitions** (geometry only). | Robert’s Rules / standard side-pot algorithm descriptions. | `committed[]` per player → pot layers + per-player caps | M | — |
| P21 | Layered-pot chip EV from per-layer equities | Given each winner’s equity **per side pot layer**, allocate chips (waterfall / proportional rules — pick one and document). | Side pot + equity combination; linear algebra. | Layer equities + P20 structure → EV per player | L | P20 |
| P22 | Exact HU equity vs uniform random hand | Enumerate all `C(50,2)` villain combos (hero known), run existing evaluator to showdown — **zero MC variance** for this restricted problem. | Brute-force equity in poker software. | `holeCards`, `board` → equity | M | Hand evaluator |
| P23 | Chubukov-style shove threshold (HU vs random) | Using P22, binary-search maximum stack (chips or BB) where open jam is still ≥ 0 EV if villain **always calls** (baseline **no fold equity**). | Chubukov / Sklansky–Chubukov push literature. | Hero hand, board stage, BB size → max profitable jam stack | L | P22 |
| P24 | Beta–Binomial posterior for fold frequency | Conjugate update for a Bernoulli “fold” rate after observing folds/calls in a stationary model. | Bayesian inference primer. | prior `(α,β)`, folds `f`, calls `c` → posterior mean / params | S | — |
| P25 | Duplication-aware effective outs (heuristic) | Down-weight raw outs when multiple opponents may share draws; **must** document as heuristic, not exact multiway equity. | Multiway draw “discount” folklore; keep assumptions explicit. | `outs`, `num_villains`, tuning params → effective outs | S | — |

**Non-goals** (explicitly out of scope for this roadmap unless the project expands): rebranded pot-odds helpers, opaque “hand power” indices, PLO/short-deck without deck and evaluator changes, hidden-approximation “GTO solvers.”

---

## How to add a math feature (contributor guide)

Follow the same pipeline the project already uses for chip / GTO math.

### Step 1 — Pure C++ API

1. Declare the function in [`include/poker/poker_math.hpp`](include/poker/poker_math.hpp) **or** add a focused header (e.g. `include/poker/icm.hpp`) if the feature set is large (ICM P17–P19).
2. Implement in [`src/poker_math.cpp`](src/poker_math.cpp) (or matching `src/icm.cpp`).
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
2. For esoteric features, prefer **one short paragraph** in README plus a longer “Assumptions / limitations” subsection in this file or in a dedicated doc if the feature grows.
3. When a feature ships, add it to [`FEATURES_ADDED.md`](FEATURES_ADDED.md) and update or remove its row from the table above.

### Step 6 — Release discipline

- Breaking changes to names or semantics require a semver decision and changelog note in release notes (see README for prior breaking-change pattern).

---

## Implementation notes (risk register)

| Topic | Note |
| --- | --- |
| **ICM (P17–P19)** | Harville is standard but not the only ICM story; document elimination-order assumptions and expose payout vector explicitly. |
| **P22–P23** | Enumeration is bounded (~1225 matchups pre-flop vs random) but still CPU work; document complexity and consider caching only at application layer, not silently inside the addon. |
| **P25** | Heuristic only — never present as exact multiway equity. |
| **P2 / P4** | Easy to get wrong with overlapping outs; prefer tests with small toy decks **or** fully specified out sets from integration tests. |

---

*This document was produced to match the repository state at the time of writing; always verify against `index.d.ts` and `README.md` before implementing.*
