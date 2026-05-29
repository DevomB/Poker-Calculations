#include "poker/range_equity.hpp"

#include "poker/cancel.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace poker {

namespace {

void fill_seven_from_indices(int h0, int h1, const std::vector<Card>& board, const int* run, int run_k,
                             std::uint8_t* ranks, std::uint8_t* suits) {
    ranks[0] = static_cast<std::uint8_t>(h0 / 4);
    suits[0] = static_cast<std::uint8_t>(h0 % 4);
    ranks[1] = static_cast<std::uint8_t>(h1 / 4);
    suits[1] = static_cast<std::uint8_t>(h1 % 4);
    const std::size_t board_n = board.size();
    for (std::size_t bi = 0; bi < board_n; ++bi) {
        ranks[2 + bi] = board[bi].rank();
        suits[2 + bi] = board[bi].suit();
    }
    const std::size_t base = 2 + board_n;
    for (int ri = 0; ri < run_k; ++ri) {
        const int idx = run[ri];
        ranks[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx / 4);
        suits[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx % 4);
    }
}

double accumulate_showdown_weighted(const std::vector<Card>& hero, const std::vector<Card>& board,
                                    int need_board, const std::vector<int>& deck_after_villain,
                                    int v0, int v1, double combo_weight,
                                    const CancelPredicate* cancel) {
    double win_weight = 0.0;
    double total = 0.0;
    std::uint8_t hero_r[7]{};
    std::uint8_t hero_s[7]{};
    std::uint8_t vil_r[7]{};
    std::uint8_t vil_s[7]{};
    const int h0 = deck_index_from_card(hero[0]);
    const int h1 = deck_index_from_card(hero[1]);

    if (need_board == 0) {
        fill_seven_from_indices(h0, h1, board, nullptr, 0, hero_r, hero_s);
        fill_seven_from_indices(v0, v1, board, nullptr, 0, vil_r, vil_s);
        const int cmp = compare_seven_strength_fast(hero_r, hero_s, vil_r, vil_s);
        total = combo_weight;
        if (cmp > 0) {
            win_weight = combo_weight;
        } else if (cmp == 0) {
            win_weight = 0.5 * combo_weight;
        }
        return win_weight / total;
    }

    for_each_combo_indices(deck_after_villain, need_board, [&](const int* run, int run_k) {
        throw_if_cancelled(cancel);
        fill_seven_from_indices(h0, h1, board, run, run_k, hero_r, hero_s);
        fill_seven_from_indices(v0, v1, board, run, run_k, vil_r, vil_s);
        const int cmp = compare_seven_strength_fast(hero_r, hero_s, vil_r, vil_s);
        total += combo_weight;
        if (cmp > 0) {
            win_weight += combo_weight;
        } else if (cmp == 0) {
            win_weight += 0.5 * combo_weight;
        }
    });
    return total > 0.0 ? win_weight / total : 0.0;
}

double equity_vs_villain_combo(const std::vector<Card>& hero, const std::vector<Card>& board,
                               int v0, int v1, const DeckBitset& used_base, int need_board,
                               const CancelPredicate* cancel) {
    DeckBitset used = used_base;
    used.set(v0);
    used.set(v1);
    const std::vector<int> after_villain = used.unused_indices();
    return accumulate_showdown_weighted(hero, board, need_board, after_villain, v0, v1, 1.0, cancel);
}

void validate_hero_board(const std::vector<Card>& hero, const std::vector<Card>& board) {
    if (hero.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (board.size() > 5) {
        throw std::invalid_argument("board must have at most 5 cards");
    }
}

}  // namespace

double exact_hu_equity_vs_known_hand(const std::vector<Card>& hero_hole_cards,
                                     const std::vector<Card>& villain_hole_cards,
                                     const std::vector<Card>& board_cards,
                                     const CancelPredicate* should_cancel) {
    validate_hero_board(hero_hole_cards, board_cards);
    if (villain_hole_cards.size() != 2) {
        throw std::invalid_argument("villain must have exactly two cards");
    }
    DeckBitset used;
    used.mark_cards(hero_hole_cards);
    used.mark_cards(board_cards);
    const int v0 = deck_index_from_card(villain_hole_cards[0]);
    const int v1 = deck_index_from_card(villain_hole_cards[1]);
    if (used.test(v0) || used.test(v1)) {
        throw std::invalid_argument("duplicate card in hero, board, or villain");
    }
    const int need_board = 5 - static_cast<int>(board_cards.size());
    return equity_vs_villain_combo(hero_hole_cards, board_cards, v0, v1, used, need_board,
                                   should_cancel);
}

double exact_hu_equity_vs_range(const std::vector<Card>& hero_hole_cards,
                                const std::vector<Card>& board_cards, const SparseRange& villain_range,
                                const CancelPredicate* should_cancel) {
    validate_hero_board(hero_hole_cards, board_cards);
    if (villain_range.combos.empty() || villain_range.weight_sum <= 0.0) {
        throw std::invalid_argument("villain range must have positive weight");
    }
    DeckBitset used;
    used.mark_cards(hero_hole_cards);
    used.mark_cards(board_cards);
    const int need_board = 5 - static_cast<int>(board_cards.size());
    double win_weight = 0.0;
    double total = 0.0;
    throw_if_cancelled(should_cancel);

    for (const WeightedHoleCombo& combo : villain_range.combos) {
        throw_if_cancelled(should_cancel);
        if (used.test(combo.card_a) || used.test(combo.card_b)) {
            continue;
        }
        DeckBitset u2 = used;
        u2.set(combo.card_a);
        u2.set(combo.card_b);
        const std::vector<int> after_villain = u2.unused_indices();
        const double w = combo.weight;
        std::uint8_t hero_r[7]{};
        std::uint8_t hero_s[7]{};
        std::uint8_t vil_r[7]{};
        std::uint8_t vil_s[7]{};
        const int h0 = deck_index_from_card(hero_hole_cards[0]);
        const int h1 = deck_index_from_card(hero_hole_cards[1]);

        if (need_board == 0) {
            fill_seven_from_indices(h0, h1, board_cards, nullptr, 0, hero_r, hero_s);
            fill_seven_from_indices(combo.card_a, combo.card_b, board_cards, nullptr, 0, vil_r,
                                    vil_s);
            const int cmp = compare_seven_strength_fast(hero_r, hero_s, vil_r, vil_s);
            total += w;
            if (cmp > 0) {
                win_weight += w;
            } else if (cmp == 0) {
                win_weight += 0.5 * w;
            }
            continue;
        }

        for_each_combo_indices(after_villain, need_board, [&](const int* run, int run_k) {
            throw_if_cancelled(should_cancel);
            fill_seven_from_indices(h0, h1, board_cards, run, run_k, hero_r, hero_s);
            fill_seven_from_indices(combo.card_a, combo.card_b, board_cards, run, run_k, vil_r,
                                    vil_s);
            const int cmp = compare_seven_strength_fast(hero_r, hero_s, vil_r, vil_s);
            total += w;
            if (cmp > 0) {
                win_weight += w;
            } else if (cmp == 0) {
                win_weight += 0.5 * w;
            }
        });
    }
    if (total <= 0.0) {
        throw std::invalid_argument("exactHuEquityVsRange: empty enumeration");
    }
    return win_weight / total;
}

double equity_delta_if_card_removed(const std::vector<Card>& hero_hole_cards,
                                    const std::vector<Card>& board_cards, int removed_deck_index,
                                    const SparseRange& villain_range,
                                    const CancelPredicate* should_cancel) {
    if (removed_deck_index < 0 || removed_deck_index > 51) {
        throw std::invalid_argument("removedDeckIndex must be 0..51");
    }
    const double base =
        exact_hu_equity_vs_range(hero_hole_cards, board_cards, villain_range, should_cancel);
    DeckBitset extra_dead;
    extra_dead.mark_cards(hero_hole_cards);
    extra_dead.mark_cards(board_cards);
    extra_dead.set(removed_deck_index);
    const std::uint64_t dead = extra_dead.mask;
    SparseRange filtered;
    filtered.weight_sum = 0.0;
    for (const WeightedHoleCombo& c : villain_range.combos) {
        if ((dead & (std::uint64_t{1} << c.card_a)) != 0 || (dead & (std::uint64_t{1} << c.card_b)) != 0) {
            continue;
        }
        filtered.combos.push_back(c);
        filtered.weight_sum += c.weight;
    }
    if (filtered.combos.empty()) {
        throw std::invalid_argument("equityDeltaIfCardRemoved: no range combos after removal");
    }
    const double after =
        exact_hu_equity_vs_range(hero_hole_cards, board_cards, filtered, should_cancel);
    return after - base;
}

}  // namespace poker
