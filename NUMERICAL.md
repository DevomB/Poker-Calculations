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

## Complexity (exact HU)

| Board | Villain | Runouts |
|-------|---------|---------|
| 0 (preflop) | random 2 cards | C(50,2)×C(48,5) |
| 3 (flop) | random 2 cards | C(47,2)×C(45,2) |
| 5 (river) | random 2 cards | C(45,2) |

`exactHuEquityVsRange` costs O(|range| × runouts per combo). Sparse ranges skip zero-weight combos.

## Floating point

- Most APIs return `double`; batch MC paths may use `float` internally then widen.
- Wilson intervals on `simulateHandOutcomeDetailed` use `wilsonScoreInterval` with `successes = round(estimate × n)`.

## Preflop matrix

`buildPreflopEquityMatrix` uses Monte Carlo with fixed villain holes per cell (disjoint suit assignment). Matrix entries satisfy `M[j,i] = 1 - M[i,j]` for `i ≠ j`.

## ICM Weitzman

`icmExpectedPayoutsWeitzman` splits each prize tier independently with weight `stack^alpha` (default `alpha = 2`). This is an independent chip-utility model, not Harville placement.
