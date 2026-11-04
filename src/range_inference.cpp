#include "poker/range_inference.hpp"

#include "poker/deck_bitset.hpp"
#include "poker/range.hpp"

#include <cmath>
#include <stdexcept>

namespace poker {

namespace {

MaterializedRangeResult from_sparse(const SparseRange& range) {
    MaterializedRangeResult out;
    out.live_combo_count = static_cast<int>(range.combos.size());
    for (const WeightedHoleCombo& c : range.combos) {
        const int idx = combo_index_1326_unchecked(c.card_a, c.card_b);
        out.weights[static_cast<std::size_t>(idx)] = c.weight;  // builders merge repeated combos
    }
    // Total in the caller's units, summed in array order so it equals the sum of `weights`.
    for (double w : out.weights) {
        out.weight_sum += w;
    }
    if (!std::isfinite(out.weight_sum)) {
        throw std::invalid_argument(
            "range weights are too large: their total exceeds the largest double (scale them down)");
    }
    // Entropy of the normalized distribution, from weights scaled by their maximum (exact).
    const int scale = weight_scale_exponent(max_combo_weight(range.combos));
    double scaled_sum = 0.0;
    for (double w : out.weights) {
        scaled_sum += std::ldexp(w, scale);
    }
    if (scaled_sum > 0.0) {
        for (double w : out.weights) {
            const double p = std::ldexp(w, scale) / scaled_sum;
            if (p > 0.0) {
                out.shannon_entropy -= p * std::log(p);
            }
        }
    }
    return out;
}

}  // namespace

MaterializedRangeResult materialize_villain_range_after_blockers(
    const double* dense1326, std::size_t dense_len, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, const std::vector<Card>& known_dead_cards) {
    if (dense_len != 1326) {
        throw std::invalid_argument("materializeVillainRange: dense range must have length 1326");
    }
    DeckBitset dead;
    dead.mark_cards_distinct(hero_hole_cards, "hero, board, or dead cards");
    dead.mark_cards_distinct(board_cards, "hero, board, or dead cards");
    dead.mark_cards_distinct(known_dead_cards, "hero, board, or dead cards");
    const SparseRange sparse = sparse_range_from_dense1326(dense1326, dense_len, dead.mask);
    return from_sparse(sparse);
}

MaterializedRangeResult materialize_villain_range_after_blockers_sparse(
    const SparseRange& prior, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, const std::vector<Card>& known_dead_cards) {
    DeckBitset dead;
    dead.mark_cards_distinct(hero_hole_cards, "hero, board, or dead cards");
    dead.mark_cards_distinct(board_cards, "hero, board, or dead cards");
    dead.mark_cards_distinct(known_dead_cards, "hero, board, or dead cards");
    SparseRange filtered;
    filtered.weight_sum = 0.0;
    for (const WeightedHoleCombo& c : prior.combos) {
        if (dead.test(c.card_a) || dead.test(c.card_b)) {
            continue;
        }
        filtered.combos.push_back(c);
        filtered.weight_sum += c.weight;
    }
    return from_sparse(filtered);
}

}  // namespace poker
