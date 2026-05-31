#include "poker/range_inference.hpp"

#include "poker/card_string.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/range.hpp"

#include <cmath>
#include <stdexcept>

namespace poker {

namespace {

int combo_dense_index(int c0, int c1) {
    if (c0 > c1) {
        std::swap(c0, c1);
    }
    return c0 * 51 + c1;
}

double strength_score(const std::vector<Card>& hero, const std::vector<Card>& board, int ca,
                      int cb) {
    std::vector<Card> vil = {card_from_deck_index(ca), card_from_deck_index(cb)};
    const std::uint64_t s = evaluate_hand_strength_fast(vil, board);
    return static_cast<double>(s);
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

MaterializedRangeResult bayesian_range_update_from_action(
    const SparseRange& prior, const std::vector<Card>& hero_hole_cards,
    const std::vector<Card>& board_cards, BayesianActionKind action, double alpha) {
    if (alpha <= 0.0 || !std::isfinite(alpha)) {
        throw std::invalid_argument("bayesianRangeUpdate: alpha must be positive");
    }
    if (prior.combos.empty()) {
        throw std::invalid_argument("bayesianRangeUpdate: empty prior range");
    }
    double max_s = 0.0;
    double min_s = 1e300;
    std::vector<double> scores;
    scores.reserve(prior.combos.size());
    for (const WeightedHoleCombo& c : prior.combos) {
        const double s = strength_score(hero_hole_cards, board_cards, c.card_a, c.card_b);
        scores.push_back(s);
        max_s = std::max(max_s, s);
        min_s = std::min(min_s, s);
    }
    const double span = std::max(1.0, max_s - min_s);
    SparseRange post;
    post.weight_sum = 0.0;
    for (std::size_t i = 0; i < prior.combos.size(); ++i) {
        const double z = (scores[i] - min_s) / span;
        double lik = 1.0;
        switch (action) {
            case BayesianActionKind::Raise:
                lik = std::exp(alpha * z);
                break;
            case BayesianActionKind::Fold:
                lik = std::exp(-alpha * z);
                break;
            case BayesianActionKind::Call:
                lik = 1.0;
                break;
        }
        WeightedHoleCombo c = prior.combos[i];
        c.weight *= lik;
        if (c.weight > 0.0) {
            post.combos.push_back(c);
            post.weight_sum += c.weight;
        }
    }
    if (post.weight_sum <= 0.0) {
        throw std::invalid_argument("bayesianRangeUpdate: posterior weight sum is zero");
    }
    return from_sparse(post);
}

}  // namespace poker
