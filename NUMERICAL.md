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
- Bets with rake (`expectedValueRaiseWithRake` and the breakeven solvers that match it): when villain
  folds, hero's bet comes back and hero wins `pot − rake(pot + b)`; when called, hero gets
  `e × (T − rake(T)) − b` for final pot `T`. So the pure-bluff threshold is
  `b / (pot − rake(pot + b) + b)` (1/3 for a half-pot bet without rake), and the semi-bluff threshold
  solves `fe × (pot − rake(pot + b)) + (1 − fe) × (e × (T − rake(T)) − b) = 0`.
- Implied odds: a call breaks even when hero later wins `W = toCall / e − pot − toCall` more
  (`impliedBreakevenFutureWin`, floored at 0), the inverse of
  `impliedOddsRequiredEquityFromFutureWin(pot, toCall, W) = toCall / (pot + toCall + W)`.
- Polarized bets: a bluff catcher calling `b` to win `pot + b` is indifferent when bluffs make up
  `b / (pot + 2b)` of the bets, i.e. `bluffToValueRatio = b / (pot + b)` bluffs per value bet and
  `valueToBluffRatio = (pot + b) / b` (1/2 and 2 for a pot-sized bet). With rake the caller wins
  `pot + 2b − rake − b`, so the ratio is `b / (pot + b − rake)`.
- Multi-street pure bluffs: a fold on street *k* wins the pot plus villain's earlier calls; being
  called down loses the sum of hero's bets. The `*SameFoldEquity` solvers return the root of that EV.

## Runout statistics against a range

A future board is not uniform once villain's range is known: the cards villain holds cannot come.
Given hero, the board, and range weights `w(h)`, a completion `r` has probability proportional to
`m(r) = Σ_{h ∩ r = ∅} w(h)` (every compatible hand leaves the same number of completions), and
hero's equity on it is the exact equity against the hands compatible with `r`. Weighting by `m(r)`
makes the average equity over runouts equal the direct equity on the current board.
`exactHeroEquityRunoutQuantiles` with a range reports these predictive statistics (runouts with
`m(r) = 0` cannot happen and are skipped); against one random hand every `m(r)` is equal.
`exactInformationRegretVsClairvoyant` is `Σ_r m(r) max(0, EV_call(e_r)) / Σ_r m(r) −
max(0, EV_call(e))`: both sides may fold, so it is never negative and is 0 when no runout makes a
call profitable.

## PKO knockout matrix

`pkoKnockoutProbabilityMatrix[i][j] = P(j busts) × w_i / Σ_k w_k` with `P(j busts) = 1 − stack_j / total`
(Harville first place) and `w_i = stack_i × stack_i / (stack_i + stack_j)` over the players who cover j
(all others when nobody covers). Columns sum to `P(j busts)`, so expected total bounty payout is
`Σ_j bounty_j × (1 − stack_j / total)`: every bounty except the eventual winner's own.

## Tournament duel

`tournamentDuelAbsorptionProbabilities` is closed-form gambler's ruin with `h = ceil(heroStack / chipsPerAllIn)`
and `v = ceil(villainStack / chipsPerAllIn)` steps, `N = h + v`, `r = q / p = e^L`:
`P(hero) = (1 − r^h) / (1 − r^N)` and `E[all-ins] = (h − N P) / (q − p)` (`h / N` and `h v` at `p = 0.5`).
`r^N` overflows in long uneven duels, so `L = log1p((1 − 2p) / p)` is used with `expm1`: for an
underdog `P = e^{−vL} expm1(−hL) / expm1(−NL)`, otherwise `expm1(hL) / expm1(NL)`; when `|L| N / 2 ≤ 1`,
`h − N P` is formed from `sinh(x)/x` differences to avoid cancellation. Example: 1000 vs 1000 one-chip
all-ins at `p = 0.1` gives `P` ≈ 0 and 1250 expected all-ins.

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

Trial planners return the smallest count meeting their bound and reject counts above 2^53 − 1:
`monteCarloTrialsForStandardErrorBound` checks `sqrt(p(1 − p)/n)` itself at the answer and one below;
`monteCarloTrialsForWilsonHalfWidth` searches the Wilson half-width `z √(pq n + z²/4) / (n + z²)`,
which strictly decreases in `n`, so the answer `n` meets the target and `n − 1` does not
(`p = 0.001`, half-width 0.01 at z = 1.96: 209 trials); `monteCarloTrialsForHoeffdingBound` is
`ceil(ln(2/δ) / (2ε²))`.

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

## Calls into side pots (`decideAction`)

A call is priced on the pot layers it can win. Hero's call (its stack, if that is less than
`toCall`) is added to the commitments and `side_pot_ladder_from_commitments` splits them into
layers; layers above hero's level are side pots hero cannot win or chips returned uncalled. Each
layer is contested by the players who put chips into it and have not folded, and those sets are
nested, so one simulation deals random hands to the deepest set and scores hero against each
layer's players. The call's value is `Σ_L chips_L × share_L − cost`; the equity compared with the
pot odds is `Σ_L chips_L × share_L / Σ_L chips_L`. Chips from earlier streets form one layer
contested by every player still in the hand, unless `totalCommittedHand` is consistent
(each at least `committedThisStreet` and summing to `pot`), in which case whole-hand commitments
are layered. When every layer is contested by exactly `monteCarloVillains` opponents this is the
plain equity estimate applied to the pot, as for heads-up spots.

Example (royal flush on board, so everyone ties): commitments `[10, 1, 100]`, hero calls 90 with 90
behind. The main pot is 3 chips three ways and the side pot 198 chips two ways, so hero collects
`1 + 99 = 100` for 90. One three-way equity (1/3) on the whole 201-chip pot would value the call at
`67 − 90 = −23`. Short stacks follow the same rule: villain bets 100 into 10 and hero has 10 left, so
hero puts in 10 to contest 30 chips (the 10 in the middle, 10 of villain's bet, and its own 10; the
other 90 go back to villain). At 40% equity that is `0.4 × 30 − 10 = +2`, where the raw
`pot = 110, toCall = 100` would give −16.

## No-limit betting rules

`legalActionSummary`, `decideAction`, and the C++ `GameEngine` share one rule set:

- A full raise is at least the last full bet or raise this street (never below the big blind).
  The minimum raise-to is `currentBet + that increment` (formed in 64 bits), or all-in when the
  stack is shorter.
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

`buildPreflopEquityMatrix` uses Monte Carlo with one representative combo per class and fixed
villain holes per cell (disjoint suit assignment), on the calling thread (`threads` is ignored).
Matrix entries satisfy `M[j,i] = 1 - M[i,j]` for `i ≠ j`.

## ICM (Harville)

Each place goes to a remaining player with probability proportional to their stack among the
players still unplaced. Placement probabilities are accumulated over the set of players already
placed (not over finishing orders), and each step's shares are scaled by the largest remaining
stack, so huge or tiny stacks keep their proportions after bigger stacks are placed
(`([1e308, 1e-308, 1e-308], [100, 50, 0])` gives `[100, 25, 25]`). Payouts only resolve the places
paying more than last place (`EV_i = last + Σ_{j<K} P(i at j)(pay_j − last)`), so equal prizes need
no enumeration and first place is each stack's share. The work is bounded: every place for up to 20
players (2^20 states), or fewer resolved places in fields up to 31 as long as `Σ_{j<K} C(n, j) ≤ 2^20`.
The skill-adjusted variant uses blended first-place weights; seats whose weights are all zero among
the remaining players finish in a uniformly random order. `icmHarvilleStackJacobian` uses central
differences with a step of `ε^{1/3} × stack` (about 6e-6 relative); its columns sum to 0 and each
row satisfies `Σ_j s_j ∂EV_i/∂s_j = 0`.

`icmExpectedPayoutsWeitzman` is deprecated: it splits the whole prize pool by `stack^alpha`
(`alpha = 2` by default) and ignores finishing positions, so it is not a payout model
(`([1, 3], [100, 100])` gives `[20, 180]` although both players are guaranteed 100).

## PKO terminals

`stacks` are chips behind and `pot` is already in the middle. In `pkoCallEvVsShove` and
`pkoJamEvVsFold` a fold gives the pot to the villain, a called all-in moves the covered amount from
each player plus the pot to the winner (bounty to the winner if the loser busts), and every terminal is
valued by Harville ICM plus expected bounties. `pkoCoveringHuntEv`'s check-down puts no more chips in:
the pot goes to the showdown winner at the same equity. Expected bounty collection uses the
stack-ratio knockout matrix above, a simplified model.

## Mystery bounties

Each value is one bounty; a draw takes a remaining bounty with probability proportional to its weight,
without replacement. `sampleK` is `Σ_i value_i × P(i among k draws)`: `k × mean` for equal weights,
otherwise the Harville top-k inclusion probabilities (same subset bound as ICM). It never exceeds the
pool and reaches it at `k = n`.

## River CFR

`cfrRiverBetCallFoldSolve` / `solveHuRiverCheckBetTree` run simultaneous vanilla CFR. Bettor values
are averaged over the defender hands each bettor hand can face (a constant chance weight per hand);
defender regrets use counterfactual call values weighted by the bettor's bet frequencies (opponent
reach), divided only by the chance mass, so each iteration's regret keeps its reach weight. Best
responses compare conditional values. On a value hand vs a bluff catcher (pot 100, bet 50) the
averages approach bet 2/3, call 2/3, bettor EV 66 2/3.

## Nash push/fold

Stacks include posted blinds and the ante is dead money. A called jam commits the smaller total
stack `M` from each player (the bigger stack's excess is returned), so the pot is `2M + ante` and each
player's call costs `M` minus their own blind. `heroEv` / `villainEv` are root values over one joint
deal of hand classes: final minus starting stack in chip mode (they sum to the ante) or ICM prize EV
(they sum to the pool less the other seats' share). Equities come from a class matrix with one
representative combo per class (class blockers and suits are approximated); the matrix cache hands
each solve an immutable snapshot, so concurrent solves with different settings do not interfere.

## Flop EHS2 buckets

Building blocks for bucketed flop abstractions; the package does not ship a flop solver.

- `ehs2BucketsVsRange` assigns each of the 1326 hero combos to one of K equal-width EHS2 buckets vs the opponent range (default K = `flopBucketCountDefault()` = 20). Board-blocked combos are `-1`.
- `bucketMassFromRange` sums a 1326 range into K bucket masses; `flopBucketStrategyTo1326` copies a per-bucket strategy back onto every combo in that bucket. Combo-level blockers between buckets are ignored.
- `canonicalFlopCfrKey` hashes `isomorphicFlopIndex` / `countCanonicalFlops` (1755) plus millichip pot and stack so suit-isomorphic flops share a cache key.
