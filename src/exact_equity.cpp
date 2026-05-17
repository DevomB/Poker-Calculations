#include "poker/exact_equity.hpp"

#include "poker/hand_evaluator.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace poker {

namespace {

[[nodiscard]] int card_to_index(const Card& c) {
    return static_cast<int>(c.rank()) * 4 + static_cast<int>(c.suit());
}

void mark_used(const std::vector<Card>& cards, std::array<bool, 52>& used) {
    for (const Card& c : cards) {
        const int idx = card_to_index(c);
        if (idx < 0 || idx >= 52) {
            throw std::invalid_argument("invalid card index");
        }
        if (used[static_cast<std::size_t>(idx)]) {
            throw std::invalid_argument("duplicate card in hero or board");
        }
        used[static_cast<std::size_t>(idx)] = true;
    }
}

[[nodiscard]] std::vector<Card> index_vector_to_cards(const std::vector<int>& indices) {
    std::vector<Card> out;
    out.reserve(indices.size());
    for (int idx : indices) {
        const int rank = idx / 4;
        const int suit = idx % 4;
        out.emplace_back(static_cast<std::uint8_t>(rank), static_cast<std::uint8_t>(suit));
    }
    return out;
}

[[nodiscard]] std::vector<int> unused_indices(const std::array<bool, 52>& used) {
    std::vector<int> d;
    d.reserve(52);
    for (int i = 0; i < 52; ++i) {
        if (!used[static_cast<std::size_t>(i)]) {
            d.push_back(i);
        }
    }
    return d;
}

void enumerate_combos(const std::vector<int>& pool, int k, int start, std::vector<int>& cur,
                      std::vector<std::vector<int>>& out) {
    if (static_cast<int>(cur.size()) == k) {
        out.push_back(cur);
        return;
    }
    const int need = k - static_cast<int>(cur.size());
    for (int i = start; i <= static_cast<int>(pool.size()) - need; ++i) {
        cur.push_back(pool[static_cast<std::size_t>(i)]);
        enumerate_combos(pool, k, i + 1, cur, out);
        cur.pop_back();
    }
}

}  // namespace

double exact_hu_equity_vs_random_hand(const std::vector<Card>& hero_hole_cards,
                                     const std::vector<Card>& board_cards) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("exactHuEquityVsRandomHand: hero must have exactly two cards");
    }
    if (board_cards.size() < 3 || board_cards.size() > 5) {
        throw std::invalid_argument(
            "exactHuEquityVsRandomHand: board must have 3..5 cards (enumerate runouts)");
    }
    std::array<bool, 52> used{};
    used.fill(false);
    mark_used(hero_hole_cards, used);
    mark_used(board_cards, used);
    const std::vector<int> deck = unused_indices(used);
    const int need_board = 5 - static_cast<int>(board_cards.size());
    const int need_villain = 2;
    if (static_cast<int>(deck.size()) < need_villain + need_board) {
        throw std::invalid_argument("not enough unknown cards for enumeration");
    }
    std::vector<std::vector<int>> villain_combos;
    std::vector<int> cur;
    enumerate_combos(deck, need_villain, 0, cur, villain_combos);
    double win_weight = 0.0;
    double total = 0.0;
    std::vector<Card> full_board;
    full_board.reserve(5);
    for (const std::vector<int>& vc : villain_combos) {
        std::array<bool, 52> u2 = used;
        for (int idx : vc) {
            u2[static_cast<std::size_t>(idx)] = true;
        }
        std::vector<int> after_villain = unused_indices(u2);
        std::vector<std::vector<int>> runouts;
        cur.clear();
        enumerate_combos(after_villain, need_board, 0, cur, runouts);
        const std::vector<Card> villain_cards = index_vector_to_cards(vc);
        for (const std::vector<int>& run : runouts) {
            full_board = board_cards;
            for (int idx : run) {
                const int rank = idx / 4;
                const int suit = idx % 4;
                full_board.emplace_back(static_cast<std::uint8_t>(rank),
                                        static_cast<std::uint8_t>(suit));
            }
            std::vector<Card> hero_cards = hero_hole_cards;
            hero_cards.insert(hero_cards.end(), full_board.begin(), full_board.end());
            std::vector<Card> vil_cards = villain_cards;
            vil_cards.insert(vil_cards.end(), full_board.begin(), full_board.end());
            const HandEvaluation h1 = evaluate_best_hand(hero_cards);
            const HandEvaluation h2 = evaluate_best_hand(vil_cards);
            total += 1.0;
            if (h2 < h1) {
                win_weight += 1.0;
            } else if (h1 == h2) {
                win_weight += 0.5;
            }
        }
    }
    if (total <= 0.0) {
        throw std::invalid_argument("exactHuEquityVsRandomHand: empty enumeration");
    }
    return win_weight / total;
}

}  // namespace poker
