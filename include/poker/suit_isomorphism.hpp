#pragma once

#include "poker/card.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace poker {

/// Unordered 3-card Hold'em flops collapse to this many suit-isomorphism classes.
inline constexpr int kCanonicalFlopCount = 1755;
inline constexpr int kSuitCount = 4;
inline constexpr int kCombo1326 = 1326;

/// `perm[old_suit] = new_suit`. Suits are 0=c, 1=d, 2=h, 3=s (same as `deck_index`).
using SuitPerm = std::array<int, kSuitCount>;

struct CanonicalHolesAndBoard {
    std::vector<Card> holes;
    std::vector<Card> board;
    SuitPerm perm{};
};

[[nodiscard]] bool is_valid_suit_perm(const SuitPerm& perm);

/// Apply `perm` to each card; input order is preserved (no sort).
[[nodiscard]] std::vector<Card> apply_suit_perm_to_cards(const std::vector<Card>& cards, const SuitPerm& perm);

/**
 * Suit permutation that maps `flop` (exactly 3 cards) onto its canonical representative.
 * Among the 24 elements of S4, pick the lex-smallest remapped flop (sorted deck ids);
 * leftover symmetry (unused suits, interchangeable pair suits) breaks by lex-smallest perm.
 */
[[nodiscard]] SuitPerm suit_perm_from_canonical_flop(const std::vector<Card>& flop);

/// Canonical 3-card flop: sorted remapped cards. Rainbow / two-tone / monotone included.
[[nodiscard]] std::vector<Card> canonical_flop_board(const std::vector<Card>& flop);

/**
 * Canonical 3–5 card board. First 3 cards are the flop (unordered); 4th is turn; 5th is river.
 * Re-solves S4 on the full street tuple (does not freeze the flop perm then greedily assign
 * leftover suits). Equivalent to flop-first iso: any perm that fails to canonicalize the flop
 * loses on the first three sorted ids; turn/river break remaining flop symmetry.
 */
[[nodiscard]] std::vector<Card> canonical_board(const std::vector<Card>& board);

/**
 * Joint canonicalization of hero holes (2) and a 3–5 card board. Same S4 search as
 * `canonical_board`, then leftover symmetry is broken by the sorted remapped holes.
 * `perm` is the winning map so the same relabel can be applied to a 1326 range.
 */
[[nodiscard]] CanonicalHolesAndBoard canonical_holes_and_board(const std::vector<Card>& holes,
                                                               const std::vector<Card>& board);

/// Dense 1326 weights permuted by mapping both hole cards through `perm`. Total mass preserved.
[[nodiscard]] std::vector<double> apply_suit_perm_to_range1326(const double* weights, std::size_t len,
                                                               const SuitPerm& perm);

/**
 * How many unordered raw flops map to this flop's isomorphism class (orbit size under S4).
 * Unpaired rainbow (3 ranks, 3 suits): 24. Unpaired two-tone: 12. Unpaired monotone: 4.
 * Rank ties (pair / trips) shrink the orbit further (12 or 4); S4 orbit sizes divide 24.
 */
[[nodiscard]] int isomorphic_flop_orbit_size(const std::vector<Card>& flop);

/// 1755 after enumerating C(52,3) and collapsing by suit iso.
[[nodiscard]] int count_canonical_flops();

/// Stable index 0..1754 of the canonical flop (sorted packed deck-id order).
[[nodiscard]] int isomorphic_flop_index(const std::vector<Card>& flop);

/// Inverse of `isomorphic_flop_index`.
[[nodiscard]] std::vector<Card> flop_index_to_canonical(int index);

}  // namespace poker
