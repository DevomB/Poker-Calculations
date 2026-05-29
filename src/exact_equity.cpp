#include "poker/exact_equity.hpp"

#include "poker/cancel.hpp"
#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/poker_math.hpp"
#include "poker/types.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace poker {

namespace {

void fill_seven_from_hole_board_run(const std::vector<Card>& hole, const std::vector<Card>& board,
                                    const int* run, int run_k, std::uint8_t* ranks, std::uint8_t* suits) {
    ranks[0] = hole[0].rank();
    suits[0] = hole[0].suit();
    ranks[1] = hole[1].rank();
    suits[1] = hole[1].suit();
    for (std::size_t bi = 0; bi < board.size(); ++bi) {
        ranks[2 + bi] = board[bi].rank();
        suits[2 + bi] = board[bi].suit();
    }
    const std::size_t base = 2 + board.size();
    for (int ri = 0; ri < run_k; ++ri) {
        const int idx = run[ri];
        ranks[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx / 4);
        suits[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx % 4);
    }
}

}  // namespace

double exact_hu_equity_vs_random_hand(const std::vector<Card>& hero_hole_cards,
                                     const std::vector<Card>& board_cards,
                                     const CancelPredicate* should_cancel) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("exactHuEquityVsRandomHand: hero must have exactly two cards");
    }
    if (board_cards.size() > 5) {
        throw std::invalid_argument("exactHuEquityVsRandomHand: board must have at most 5 cards");
    }
    if (board_cards.size() < 3 && board_cards.size() != 0) {
        throw std::invalid_argument(
            "exactHuEquityVsRandomHand: board must be empty (preflop) or have 3..5 cards");
    }
    DeckBitset used;
    used.mark_cards(hero_hole_cards);
    used.mark_cards(board_cards);
    const std::vector<int> deck = used.unused_indices();
    const int need_board = 5 - static_cast<int>(board_cards.size());
    const int need_villain = 2;
    if (static_cast<int>(deck.size()) < need_villain + need_board) {
        throw std::invalid_argument("not enough unknown cards for enumeration");
    }
    double win_weight = 0.0;
    double total = 0.0;
    throw_if_cancelled(should_cancel);
    const int h0 = deck_index_from_card(hero_hole_cards[0]);
    const int h1 = deck_index_from_card(hero_hole_cards[1]);
    std::uint8_t hero_r[7]{};
    std::uint8_t hero_s[7]{};
    std::uint8_t vil_r[7]{};
    std::uint8_t vil_s[7]{};

    for_each_combo_indices(deck, need_villain, [&](const int* vc, int vc_k) {
        throw_if_cancelled(should_cancel);
        DeckBitset u2 = used;
        for (int i = 0; i < vc_k; ++i) {
            u2.set(vc[i]);
        }
        const std::vector<int> after_villain = u2.unused_indices();
        for_each_combo_indices(after_villain, need_board, [&](const int* run, int run_k) {
            throw_if_cancelled(should_cancel);
            fill_seven_from_hole_board_run(hero_hole_cards, board_cards, run, run_k, hero_r, hero_s);
            vil_r[0] = static_cast<std::uint8_t>(vc[0] / 4);
            vil_s[0] = static_cast<std::uint8_t>(vc[0] % 4);
            vil_r[1] = static_cast<std::uint8_t>(vc[1] / 4);
            vil_s[1] = static_cast<std::uint8_t>(vc[1] % 4);
            for (std::size_t bi = 0; bi < board_cards.size(); ++bi) {
                vil_r[2 + bi] = board_cards[bi].rank();
                vil_s[2 + bi] = board_cards[bi].suit();
            }
            const std::size_t base = 2 + board_cards.size();
            for (int ri = 0; ri < run_k; ++ri) {
                const int idx = run[ri];
                vil_r[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx / 4);
                vil_s[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx % 4);
            }
            const int cmp = compare_seven_strength_fast(hero_r, hero_s, vil_r, vil_s);
            total += 1.0;
            if (cmp > 0) {
                win_weight += 1.0;
            } else if (cmp == 0) {
                win_weight += 0.5;
            }
        });
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exactHuEquityVsRandomHand: empty enumeration");
    }
    return win_weight / total;
}

double straight_made_flop_to_river_exact_probability(const std::vector<Card>& hero_hole_cards,
                                                     const std::vector<Card>& flop_three_cards,
                                                     const std::vector<Card>& known_dead_cards,
                                                     const CancelPredicate* should_cancel) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("straightMadeFlopToRiverExactProbability: hero must have exactly two cards");
    }
    if (flop_three_cards.size() != 3) {
        throw std::invalid_argument("straightMadeFlopToRiverExactProbability: flop must have exactly three cards");
    }
    DeckBitset used;
    used.mark_cards(hero_hole_cards);
    used.mark_cards(flop_three_cards);
    used.mark_cards(known_dead_cards);
    const std::vector<int> deck = used.unused_indices();
    if (static_cast<int>(deck.size()) < 2) {
        throw std::invalid_argument("straightMadeFlopToRiverExactProbability: need at least two unseen cards");
    }
    std::size_t hits = 0;
    std::size_t total = 0;
    throw_if_cancelled(should_cancel);
    for_each_combo_indices(deck, 2, [&](const int* pr, int) {
        throw_if_cancelled(should_cancel);
        ++total;
        std::vector<Card> seven;
        seven.reserve(7);
        seven.insert(seven.end(), hero_hole_cards.begin(), hero_hole_cards.end());
        seven.insert(seven.end(), flop_three_cards.begin(), flop_three_cards.end());
        seven.push_back(card_from_deck_index(pr[0]));
        seven.push_back(card_from_deck_index(pr[1]));
        const HandEvaluation he = evaluate_best_hand(seven);
        const HandRank cat = hand_category(he);
        if (cat == HandRank::Straight || cat == HandRank::StraightFlush || cat == HandRank::RoyalFlush) {
            ++hits;
        }
    });
    if (total == 0) {
        throw std::invalid_argument("straightMadeFlopToRiverExactProbability: empty enumeration");
    }
    return static_cast<double>(hits) / static_cast<double>(total);
}

int chubukov_max_symmetric_jam_stack_from_hand_binary_search(const std::vector<Card>& hero_hole_cards,
                                                             const std::vector<Card>& board_cards,
                                                             double dead_money_chips, int max_stack_chips) {
    if (!std::isfinite(dead_money_chips) || dead_money_chips < 0.0) {
        throw std::invalid_argument("deadMoneyChips must be finite and non-negative");
    }
    if (max_stack_chips < 0) {
        throw std::invalid_argument("maxStackChips must be non-negative");
    }
    if (max_stack_chips == 0) {
        return 0;
    }
    const double eq = exact_hu_equity_vs_random_hand(hero_hole_cards, board_cards);
    return chubukov_max_symmetric_jam_stack_chips_binary_search(eq, dead_money_chips, max_stack_chips);
}

double chubukov_max_symmetric_jam_stack_binary_search(const std::vector<Card>& hero_hole_cards,
                                                     const std::vector<Card>& board_cards,
                                                     double dead_money_chips, double max_stack_chips,
                                                     int iterations) {
    if (!std::isfinite(dead_money_chips) || dead_money_chips < 0.0) {
        throw std::invalid_argument("deadMoneyChips must be finite and non-negative");
    }
    if (!std::isfinite(max_stack_chips) || max_stack_chips < 0.0) {
        throw std::invalid_argument("maxStackChips must be finite and non-negative");
    }
    int it = iterations;
    if (it < 1) {
        it = 1;
    }
    if (it > 200) {
        it = 200;
    }
    const double eq = exact_hu_equity_vs_random_hand(hero_hole_cards, board_cards);
    if (eq <= 0.0) {
        return 0.0;
    }
    if (eq >= 0.5) {
        return max_stack_chips;
    }
    const auto ev = [eq, dead_money_chips](double S) {
        return chubukov_symmetric_jam_ev(S, dead_money_chips, eq);
    };
    if (ev(max_stack_chips) >= 0.0) {
        return max_stack_chips;
    }
    double lo = 0.0;
    double hi = max_stack_chips;
    for (int i = 0; i < it; ++i) {
        const double mid = (lo + hi) * 0.5;
        if (ev(mid) >= 0.0) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

}  // namespace poker
