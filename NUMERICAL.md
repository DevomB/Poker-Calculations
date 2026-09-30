# Numerical semantics

## Deck indices

Cards use `deckIndex = rank * 4 + suit` with rank `0..12` (2..A) and suit `0..3` (c,d,h,s).

## Random number generation

- Monte Carlo: `std::mt19937` with user-supplied seed.
- `parallelHandSimulation`: worker `t` uses seed `baseSeed + t * 9743`.
- Cooperative cancellation checks every **4096** iterations (`CancelPredicate`).

## Showdown equity

- Multi-way: hero receives `1 / tiedAtBest` when tied for best strength; otherwise `0`.
- Heads-up exact enumeration: win `1`, chop `0.5`, loss `0`.
- Hand potential (HS / PPot / NPot): same chop. HS = P(ahead) + 0.5 P(tie) on this board.
  PPot / NPot use the Billings half-tie transitions:
  `PPot = [P(behind→ahead) + 0.5 P(behind→tied) + 0.5 P(tied→ahead)] / [P(behind) + 0.5 P(tied)]`.

## Complexity (exact HU)

| Board | Villain | Runouts |
|-------|---------|---------|
| 0 (preflop) | random 2 cards | C(50,2)×C(48,5) |
| 3 (flop) | random 2 cards | C(47,2)×C(45,2) |
| 5 (river) | random 2 cards | C(45,2) |

`exactHuEquityVsRange` costs O(|range| × runouts per combo). Sparse ranges skip zero-weight combos.

## Complexity (exact multiway known hands)

`exactMultiwayEquityKnownHands` and siblings enumerate remaining boards only (holes are known). Tie rule is the multi-way showdown rule above.

| Players | Board | Dead | Runouts |
|---------|-------|------|---------|
| 3 | 0 (preflop) | 0 | C(46,5) |
| 4 | 0 (preflop) | 0 | C(44,5) |
| 6 | 0 (preflop) | 0 | C(40,5) |
| n | 5 (river) | any | 1 |

## Pot convention (chip math)

- `pot` / `potBeforeCall` is everything in the middle when hero acts, villain's bet included.
  `expectedValueCall(e, pot, toCall) = e × pot − (1 − e) × toCall`, which is zero exactly at
  `breakevenCallEquity(pot, toCall) = toCall / (pot + toCall)`.
- Final pot after a heads-up call is `pot + toCall`; after hero bets `b` and is called it is `pot + 2b`.
  Rake variants take rake from that final pot and reduce to their unraked base at zero rake.
- Multi-street pure bluffs: a fold on street *k* wins the pot plus villain's earlier calls; being
  called down loses the sum of hero's bets. The `*SameFoldEquity` solvers return the root of that EV.

## PKO knockout matrix

`pkoKnockoutProbabilityMatrix[i][j] = P(j busts) × w_i / Σ_k w_k` with `P(j busts) = 1 − stack_j / total`
(Harville first place) and `w_i = stack_i × stack_i / (stack_i + stack_j)` over the players who cover j
(all others when nobody covers). Columns sum to `P(j busts)`, so expected total bounty payout is
`Σ_j bounty_j × (1 − stack_j / total)`: every bounty except the eventual winner's own.

## Tournament duel

`tournamentDuelAbsorptionProbabilities` is closed-form gambler's ruin with `h = ceil(heroStack / chipsPerAllIn)`
and `v = ceil(villainStack / chipsPerAllIn)` steps: `P(hero) = (1 − r^h) / (1 − r^(h+v))`, `r = (1 − p) / p`,
or `h / (h + v)` when `p = 0.5`; expected all-ins `h × v` at `p = 0.5`.

## Floating point

- Most APIs return `double`; batch MC paths may use `float` internally then widen.
- Wilson intervals on `simulateHandOutcomeDetailed` use `wilsonScoreInterval` with `successes = round(estimate × n)`.

## Preflop matrix

`buildPreflopEquityMatrix` uses Monte Carlo with fixed villain holes per cell (disjoint suit assignment). Matrix entries satisfy `M[j,i] = 1 - M[i,j]` for `i ≠ j`.

## ICM Weitzman

`icmExpectedPayoutsWeitzman` splits each prize tier independently with weight `stack^alpha` (default `alpha = 2`). This is an independent chip-utility model, not Harville placement.

## Flop EHS2 buckets

Building blocks for bucketed flop abstractions; the package does not ship a flop solver.

- `ehs2BucketsVsRange` assigns each of the 1326 hero combos to one of K equal-width EHS2 buckets vs the opponent range (default K = `flopBucketCountDefault()` = 20). Board-blocked combos are `-1`.
- `bucketMassFromRange` sums a 1326 range into K bucket masses; `flopBucketStrategyTo1326` copies a per-bucket strategy back onto every combo in that bucket. Combo-level blockers between buckets are ignored.
- `canonicalFlopCfrKey` hashes `isomorphicFlopIndex` / `countCanonicalFlops` (1755) plus millichip pot and stack so suit-isomorphic flops share a cache key.
