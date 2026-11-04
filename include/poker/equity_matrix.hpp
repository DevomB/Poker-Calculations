#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace poker {

struct PreflopMatrixOptions {
    int iterations{5000};
    std::uint32_t seed{1};
    std::size_t num_threads{1};  // ignored: the matrix is computed on the calling thread
};

/// Fill length `169 * 169` row-major: equity of hand row vs hand column (MC, symmetric triangle computed).
void build_preflop_equity_matrix(const PreflopMatrixOptions& opts, std::vector<double>& out);

inline constexpr int kHandClassCount = 169;

/// A preflop hand class. Ranks are 0 = 2 .. 12 = A, `high_rank >= low_rank`; pairs are never suited.
struct HandClass169 {
    int high_rank{0};
    int low_rank{0};
    bool suited{false};
};

/**
 * Canonical 169 order used by the preflop matrix, Nash charts, and hand-class arrays: low rank in
 * the outer loop, each block a pair then suited/offsuit pairs with each higher rank, so
 * 0 = 22, 1 = 32s, 2 = 32o, 3 = 42s, ..., 165 = KK, 166 = AKs, 167 = AKo, 168 = AA.
 * Ranks may be given in either order. Throws std::invalid_argument for ranks outside 0..12 or a
 * suited pair.
 */
[[nodiscard]] int hand169_index(int high_rank, int low_rank, bool suited);

/// Inverse of `hand169_index`; throws std::invalid_argument outside 0..168.
[[nodiscard]] HandClass169 hand169_class(int hand169);

/// Map 0..168 to two deck indices for non-overlapping sample suits; returns false if invalid.
[[nodiscard]] bool hand169_to_deck_indices(int hand169, int& c0, int& c1);

}  // namespace poker
