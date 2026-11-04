# Numerical semantics

## Deck, combo, and class indices

- Cards use `deckIndex = rank * 4 + suit` with rank `0..12` (2..A) and suit `0..3` (c,d,h,s).
- Dense 1326 ranges are lexicographic over deck-id pairs `a < b`:
  `index = a * 51 - a * (a - 1) / 2 + (b - a - 1)`, so `(0,1) -> 0`, `(1,2) -> 51`, `(50,51) -> 1325`.
- The 169 hand classes take the low rank in the outer loop, each block a pair and then suited /
  offsuit with every higher rank: `22, 32s, 32o, 42s, …, KK, AKs, AKo, AA`. With `l <= h`,
  `index = l (26 - l)` for the pair and `l (26 - l) + 1 + 2 (h - l - 1) + (offsuit ? 1 : 0)` otherwise.

## Random number generation

- Monte Carlo: `std::mt19937` with user-supplied seed.
- `parallelHandSimulation`: worker `t` uses seed `baseSeed + t * 9743`.
- `decideAction`: seed `rngSeed XOR (pot * 1315423911 mod 2^32)`; `rngSeed` itself is reduced modulo 2^32.
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

## Monte Carlo uncertainty

A trial pays hero `X` in [0, 1]: 1 for a win, `1/k` for a k-way chop, 0 for a loss. Chops make
`X` non-Bernoulli, so `simulateHandOutcomeDetailed` reports:

- `se = sqrt(s² / n)`, where `s² = Σ (x - x̄)² / (n - 1)` is the sample variance of the payoffs
  (accumulated with Welford's update). With `n = 1` there is no sample variance and `se = 0.5`,
  the largest standard deviation a [0, 1] payoff can have.
- `[ciLow, ciHigh]`: the 95% Wilson score interval evaluated at the unrounded mean `x̄`,
  `(x̄ + z²/2n ± z √(x̄(1 − x̄)/n + z²/4n²)) / (1 + z²/n)` with `z = 1.96`. Any payoff
  in [0, 1] with mean `μ` has `Var X <= μ (1 - μ)`, the Bernoulli variance the Wilson interval
  assumes, so the interval is conservative for split pots. It deliberately ignores `s²`: when every
  sample is equal (`s² = 0`, for example a board everyone plays) the true outcome distribution can
  still have rare outcomes the sample missed, and the interval keeps its width.
- Limits: both are large-sample normal approximations (coverage is approximate for small `n`),
  they quantify Monte Carlo error only, and seeds make results repeatable rather than independent.

`monteCarloStandardError(pHat, n)` stays the Bernoulli formula `sqrt(pHat (1 - pHat) / n)`.

## Range weights

A weighted range is a distribution: only ratios of weights matter. Every range input is checked
first: weights must be finite and non-negative, and sparse `indices` must be integer deck ids 0..51
(checked before any conversion). A combo listed more than once is one combo whose weight is the sum
of its entries; a sum past the largest double is rejected.

Ranges keep the caller's weights. Each calculation multiplies the weights it uses by one power of
two `2^k`, chosen so the largest weight **among the combos that take part** lands in `[0.5, 1)`:
the combos left after hero, the board, and dead cards (and, in the river CFR subgames, each
card-compatible bettor and defender pair). Scaling by a power of two is exact in binary floating
point, so ordinary ranges give bit-identical results, while totals over all combos and runouts stay
finite. A weight more than `2^1074` times smaller than the largest active weight contributes less
than one part in `2^1022` of the total, so its underflow cannot change a result. Taking the scale
from a combo that later drops out could zero every combo that remains, which is why the scale is
never global.

Sums of products are kept as mantissa and exponent. The river CFR subgames weight a pair of hands
by `w_bettor × w_defender` (for example `1e200 × 1e-200`), and the bet-node values weight bettor
combos by `w × P(bet)`; both are accumulated relative to the largest term seen so far, so each
conditional average (per hand, per hand class, per pair) is scaled by its own members.

Results reported in weight units (`handPotentialBreakdown` counts, `materializeVillainRangeAfterBlockers`
weights and `weightSum`, CFR top-combo weights, class and per-card totals) are in the caller's
units, and throw rather than return Infinity when such a total exceeds the largest double.
`materializeVillainRangeAfterBlockers` reports `weightSum` as the sum of its `weights1326` array and
`liveComboCount` as the number of positive entries.

## Multiway range equity

`simulateEquityVsRanges` targets the joint law of the opponents' hands,
`P(h₁ … hₖ) ∝ w₁(h₁) ⋯ wₖ(hₖ)` over tuples that share no card with each other, hero, the board, or
the dead cards. Each draw samples every range independently (combos touching known cards removed)
and keeps the tuple only if the hands are disjoint; on any collision the whole tuple is redrawn.
Accepted tuples have exactly the target law. Drawing hands one at a time and renormalising later
ranges after removing earlier cards does not: it weights each earlier hand by its own range rather
than by how many compatible completions it has. The board is then completed uniformly from the
remaining deck, and payoffs and uncertainty are those of the Monte Carlo uncertainty section. The
sampler stops with an error after `maxAttempts` draws (default `100 n + 10000`), so it fails rather
than slows down when the acceptance rate `n / attempts` falls below about 1%. Every accepted sample
costs at least one draw, so a `maxAttempts` below `n` is rejected up front. Results are showdown
equity only; side-pot payouts need `exactMultiwaySidePotChipEv` or the side-pot helpers.

## Floating point

- Most APIs return `double`; batch MC paths may use `float` internally then widen.

## Short-stack calls (`decideAction`)

When hero's stack `S` is below `toCall`, a call costs `S` and can only win what each player has put
in up to hero's matched level. With this street's commitments `c_j`, hero's level after the call
`h = c_hero + S`, and `side_pot_ladder_from_commitments` layers, the call is priced with
`pot' = (pot - Σ c_j) + Σ_j min(c_j, h) - S` (hero's own term counted as `h`) and `toCall' = S`;
earlier streets are fully matched by anyone who still has chips. Heads-up, villain's unmatched excess is returned. Example: villain bets
100 into 10 and hero has 10 left, so `pot' = 20`, `toCall' = 10`, and at 40% equity the call is
worth `0.4 × 20 − 0.6 × 10 = +2` (the raw `pot = 110, toCall = 100` would give −16). Hero's single
equity estimate is applied to every layer hero can win. With a full call `pot' = pot`.

## No-limit betting rules

`legalActionSummary`, `decideAction`, and the C++ `GameEngine` share one rule set:

- A full raise is at least the last full bet or raise this street (never below the big blind).
  The minimum raise-to is `currentBet + that increment`, or all-in when the stack is shorter.
- Raising needs another player still in the hand with chips behind. Against opponents who are all
  in, the choices are call (or check) and fold.
- An all-in raise short of a full raise keeps the previous increment and does not reopen betting:
  a player who already acted may call or fold, and may raise again only once the bet has grown by
  at least one full raise since their last action (short raises can add up to one).
- `GameEngine` only: heads-up the button posts the small blind and acts first before the flop; a
  short (all-in) blind still leaves the full big blind to call; once at most one player can bet the
  board runs out to showdown; one player left ends the hand. `pot + Σ stacks` is conserved.

## Opponent modeling

- `opponentAggressionFactor(bets, raises, calls) = (bets + raises) / max(1, calls)` from action counts.
- `betaBinomialFoldPosterior` / `opponentFoldToCbetPosterior`: Beta(α₀, β₀) prior, integer fold and
  continue counts, posterior Beta(α₀ + folds, β₀ + continues) with mean α / (α + β) and variance
  αβ / ((α + β)² (α + β + 1)).
- `NativeOpponentModel.aggressionFactor` is a different quantity: a normalized score in [0, 1]
  (0.5 neutral). `decideAction` adds `(aggressionFactor − 0.5) × opponentAggressionWeight` to hero's
  equity, a heuristic adjustment, not a range model.

## Preflop matrix

`buildPreflopEquityMatrix` uses Monte Carlo with fixed villain holes per cell (disjoint suit assignment). Matrix entries satisfy `M[j,i] = 1 - M[i,j]` for `i ≠ j`.

## ICM Weitzman

`icmExpectedPayoutsWeitzman` splits each prize tier independently with weight `stack^alpha` (default `alpha = 2`). This is an independent chip-utility model, not Harville placement.

## Flop EHS2 buckets

Building blocks for bucketed flop abstractions; the package does not ship a flop solver.

- `ehs2BucketsVsRange` assigns each of the 1326 hero combos to one of K equal-width EHS2 buckets vs the opponent range (default K = `flopBucketCountDefault()` = 20). Board-blocked combos are `-1`.
- `bucketMassFromRange` sums a 1326 range into K bucket masses; `flopBucketStrategyTo1326` copies a per-bucket strategy back onto every combo in that bucket. Combo-level blockers between buckets are ignored.
- `canonicalFlopCfrKey` hashes `isomorphicFlopIndex` / `countCanonicalFlops` (1755) plus millichip pot and stack so suit-isomorphic flops share a cache key.
