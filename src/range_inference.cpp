#include "poker/range_inference.hpp"

#include "poker/deck_bitset.hpp"
#include "poker/range.hpp"

#include <cmath>
#include <stdexcept>

namespace poker {

namespace {

int combo_dense_index(int c0, int c1) {
    if (c0 > c1) {
        std::swap(c0, c1);
    }
    // Lexicographic (a < b) slot in the 1326-entry dense range, matching range.cpp.
    return c0 * 51 - c0 * (c0 - 1) / 2 + (c1 - c0 - 1);
}

MaterializedRangeResult from_sparse(const SparseRange& range) {
    MaterializedRangeResult out;
    out.weight_sum = range.weight_sum;
    out.live_combo_count = static_cast<int>(range.combos.size());
    for (const WeightedHoleCombo& c : range.combos) {
        const int idx = combo_dense_index(c.card_a, c.card_b);
        out.weights[static_cast<std::size_t>(idx)] = c.weight;
    }
    if (out.weight_sum > 0.0) {
        for (double w : out.weights) {
            if (w > 0.0) {
                const double p = w / out.weight_sum;
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
    dead.mark_cards(hero_hole_cards);
    dead.mark_cards(board_cards);
    dead.mark_cards(known_dead_cards);
    const SparseRange sparse = sparse_range_from_dense1326(dense1326, dense_len, dead.mask);
    return from_sparse(sparse);
}

MaterializedRangeResult materialize_villain_range_after_blockers_sparse(
    const SparseRange& prior, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, const std::vector<Card>& known_dead_cards) {
    DeckBitset dead;
    dead.mark_cards(hero_hole_cards);
    dead.mark_cards(board_cards);
    dead.mark_cards(known_dead_cards);
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
