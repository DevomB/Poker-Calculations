#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace poker {

struct PreflopMatrixOptions {
    int iterations{5000};
    std::uint32_t seed{1};
    std::size_t num_threads{1};
};

/// Fill length `169 * 169` row-major: equity of hand row vs hand column (MC, symmetric triangle computed).
void build_preflop_equity_matrix(const PreflopMatrixOptions& opts, std::vector<double>& out);

/// Map 0..168 to two deck indices for non-overlapping sample suits; returns false if invalid.
[[nodiscard]] bool hand169_to_deck_indices(int hand169, int& c0, int& c1);

}  // namespace poker
