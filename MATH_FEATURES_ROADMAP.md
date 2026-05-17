# Math features — canonical roadmap

This file is the **single index** for shipped math, how to extend the library, and a **quality-gated** backlog of esoteric ideas. Detailed export tables: [`FEATURES_ADDED.md`](FEATURES_ADDED.md).

---

## 1. Shipped math (summary)

All **P1–P25** roadmap items from the former [`FEATURES_IN_PROGRESS.md`](FEATURES_IN_PROGRESS.md) table are **implemented** and exposed on `PokerCalculations` (see [`index.d.ts`](index.d.ts)).

| Phase | IDs | Topic |
| --- | --- | --- |
| A | P1, P3, P6, P7, P11, P12, P15, P24, P25 | Hypergeometric one-card, RR flush, reverse implied ceiling, geometric pot, Harrington M / effective M, Kelly, MC SE, Beta–Binomial, duplication heuristic |
| B | P13, P14, P16 | Diffusion ROR, bankroll for target ROR, Wilson interval |
| C | P9, P10 | Rake on final pot; call, semi-bluff, and pure-bluff breakevens with rake |
| D | P2, P4, P5, P8 | Flop–river hit (single, disjoint sum, two- and three-category unions), RR straight draw outs, multiway symmetric breakeven + share model, two-street pure bluff (same FE, general FE, second-street breakeven) |
| E | P20, P21 | Side-pot ladder, layered pot chip EV from per-layer equities |
| F | P22, P23 | Exact HU vs random (board 3–5); Chubukov closed-form stack + max-stack binary search from enumerated equity |
| G | P17, P18, P19 | Harville ICM (full placement matrix, win probs), expected payouts, pairwise bubble factor |

---

## 2. How to add a feature

Use the step-by-step pipeline in [`FEATURES_IN_PROGRESS.md`](FEATURES_IN_PROGRESS.md) (C++ → GTest → N-API → `index.d.ts` → README / `FEATURES_ADDED.md`).

---

## 3. Research backlog (N-features, quality gate)

These are **candidate** extensions **beyond P1–P25**. Each must stay non-trivial, literature-backed, and non-overlapping with shipped APIs. **Do not pad** the list: if an item fails the gate during design, drop it.

| ID | Name | One-line idea | Primary references (for formulas, not API) |
| --- | --- | --- | --- |
| N1 | Malmuth–Weitzman ICM | Alternative elimination / placement model vs Harville for sensitivity analysis | Tournament ICM literature; compare to Harville weights |
| N2 | PKO bounty $EV | Progressive knockout: bounty chip value vs tournament equity decomposition | Modern PKO tournament modeling articles |
| N3 | Harrington Q-ratio | Pressure index using stack vs **average** table stack (distinct from M) | Harrington on Hold’em tournament volumes |
| N4 | Discrete gambler’s ruin | Exact finite Markov ruin/win probabilities vs diffusion P13–P14 | Classical probability texts (Feller) |
| N5 | λ-fractional Kelly | Half/quarter Kelly and explicit link to risk-of-ruin goals | Kelly (1956); MacLean, Thorp, Ziemba |
| N6 | Asymmetric multiway breakeven | Callers contribute different amounts; generalize P5 | Multiway pot-odds extensions |
| N7 | Inclusion–exclusion draw union | Rigorous two-street probability when out **categories** overlap | Enumerative probability / combo counting |
| N8 | Clairvoyant / flip toy EV | Closed-form benchmarks from stylized heads-up games | Chen & Ankenman, *Mathematics of Poker* |
| N9 | Exponential-utility sizing | Optimal bet size under CARA utility (beyond binary Kelly) | Finance and gambling theory (Pratt / Arrow) |
| N10 | Squeeze pot geometry | Chip accounting when cold callers + squeeze / reshove lines change the ladder | Cash game geometry; may share helpers with P20 |

**Gate before shipping any N-item:** definition, inputs/outputs, edge cases, tests vs a reference (paper, small brute force, or symbolic algebra), and explicit “not a substitute for solver output” disclaimer where relevant.

---

## 4. Non-goals (unchanged)

Rebranded pot-odds-only helpers, opaque “hand power” indices, PLO/short-deck without deck+evaluator changes, hidden full-game GTO solvers — see historical notes in git history for `FEATURES_IN_PROGRESS.md` if needed.

---

*Last expanded when P1–P25 extensions (P2 union, P4 straight RR, P5 share, P8 general FE, P11 effective M, P10 pure rake, P17 placement matrix, P23 binary search) shipped; verify `index.d.ts` before relying on names.*
