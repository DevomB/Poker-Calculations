#include "poker/exact_equity.hpp"

#include "poker/cancel.hpp"
#include "poker/card_string.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/poker_math.hpp"
#include "poker/types.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace poker {

namespace {

void mark_used(const std::vector<Card>& cards, std::array<bool, 52>& used) {
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (idx < 0 || idx >= 52) {
            throw std::invalid_argument("invalid card index");
        }
        if (used[static_cast<std::size_t>(idx)]) {
            throw std::invalid_argument("duplicate card in hero or board");
        }
        used[static_cast<std::size_t>(idx)] = true;
    }
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

template <typename Fn>
void for_each_combo(const std::vector<int>& pool, int k, Fn&& fn) {
    if (k <= 0 || static_cast<int>(pool.size()) < k) {
        return;
    }
    std::vector<int> cur;
    cur.reserve(static_cast<std::size_t>(k));
    const auto go = [&](const auto& self, int start) -> void {
        if (static_cast<int>(cur.size()) == k) {
            fn(cur);
            return;
        }
        const int need = k - static_cast<int>(cur.size());
        for (int i = start; i <= static_cast<int>(pool.size()) - need; ++i) {
            cur.push_back(pool[static_cast<std::size_t>(i)]);
            self(self, i + 1);
            cur.pop_back();
        }
    };
    go(go, 0);
}

void fill_seven_from_hole_board_run(const std::vector<Card>& hole, const std::vector<Card>& board,
                                    const std::vector<int>& run, std::uint8_t* ranks, std::uint8_t* suits) {
    ranks[0] = hole[0].rank();
    suits[0] = hole[0].suit();
    ranks[1] = hole[1].rank();
    suits[1] = hole[1].suit();
    for (std::size_t bi = 0; bi < board.size(); ++bi) {
        ranks[2 + bi] = board[bi].rank();
        suits[2 + bi] = board[bi].suit();
    }
    const std::size_t base = 2 + board.size();
    for (std::size_t ri = 0; ri < run.size(); ++ri) {
        const int idx = run[ri];
        ranks[base + ri] = static_cast<std::uint8_t>(idx / 4);
        suits[base + ri] = static_cast<std::uint8_t>(idx % 4);
    }
}

}  // namespace

double exact_hu_equity_vs_random_hand(const std::vector<Card>& hero_hole_cards,
                                     const std::vector<Card>& board_cards,
                                     const CancelPredicate* should_cancel) {
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
    double win_weight = 0.0;
    double total = 0.0;
    throw_if_cancelled(should_cancel);
    for_each_combo(deck, need_villain, [&](const std::vector<int>& vc) {
        throw_if_cancelled(should_cancel);
        std::array<bool, 52> u2 = used;
        for (int idx : vc) {
            u2[static_cast<std::size_t>(idx)] = true;
        }
        const std::vector<int> after_villain = unused_indices(u2);
        for_each_combo(after_villain, need_board, [&](const std::vector<int>& run) {
            throw_if_cancelled(should_cancel);
            std::uint8_t hero_r[7]{};
            std::uint8_t hero_s[7]{};
            std::uint8_t vil_r[7]{};
            std::uint8_t vil_s[7]{};
            fill_seven_from_hole_board_run(hero_hole_cards, board_cards, run, hero_r, hero_s);
            vil_r[0] = static_cast<std::uint8_t>(vc[0] / 4);
            vil_s[0] = static_cast<std::uint8_t>(vc[0] % 4);
            vil_r[1] = static_cast<std::uint8_t>(vc[1] / 4);
            vil_s[1] = static_cast<std::uint8_t>(vc[1] % 4);
            for (std::size_t bi = 0; bi < board_cards.size(); ++bi) {
                vil_r[2 + bi] = board_cards[bi].rank();
                vil_s[2 + bi] = board_cards[bi].suit();
            }
            const std::size_t base = 2 + board_cards.size();
            for (std::size_t ri = 0; ri < run.size(); ++ri) {
                const int idx = run[ri];
                vil_r[base + ri] = static_cast<std::uint8_t>(idx / 4);
                vil_s[base + ri] = static_cast<std::uint8_t>(idx % 4);
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
    std::array<bool, 52> used{};
    used.fill(false);
    mark_used(hero_hole_cards, used);
    mark_used(flop_three_cards, used);
    mark_used(known_dead_cards, used);
    const std::vector<int> deck = unused_indices(used);
    if (static_cast<int>(deck.size()) < 2) {
        throw std::invalid_argument("straightMadeFlopToRiverExactProbability: need at least two unseen cards");
    }
    std::size_t hits = 0;
    std::size_t total = 0;
    throw_if_cancelled(should_cancel);
    for_each_combo(deck, 2, [&](const std::vector<int>& pr) {
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
