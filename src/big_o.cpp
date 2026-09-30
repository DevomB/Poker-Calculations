#include "poker/big_o.hpp"

#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace poker {
namespace {

void mark_unique(DeckBitset& used, const std::vector<Card>& cards) {
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (used.test(idx)) {
            throw std::invalid_argument("duplicate card");
        }
        used.set(idx);
    }
}

void require_hole5(const std::vector<Card>& hole) {
    if (hole.size() != 5) {
        throw std::invalid_argument("big O hole must be exactly 5 cards");
    }
}

void require_board_eval(const std::vector<Card>& board) {
    if (board.size() < 3 || board.size() > 5) {
        throw std::invalid_argument("big O evaluation needs 3..5 board cards");
    }
}

void require_board_runout(const std::vector<Card>& board) {
    if (board.size() > 5) {
        throw std::invalid_argument("big O board must have at most 5 cards");
    }
}

void cards_to_idx(const std::vector<Card>& cards, int* out) {
    for (std::size_t i = 0; i < cards.size(); ++i) {
        out[i] = deck_index_from_card(cards[i]);
    }
}

HandEvaluation big_o_best_from_idx(const int hole[5], const int* board, int board_n) {
    return plo_best_two_plus_three(hole, 5, board, board_n);
}

std::uint64_t big_o_strength_from_idx(const int hole[5], const int* board, int board_n) {
    return pack_hand_strength(big_o_best_from_idx(hole, board, board_n));
}

void split_dead(const std::vector<Card>& hole, const std::vector<Card>& board,
                const std::vector<Card>& extra_dead, DeckBitset& used) {
    require_hole5(hole);
    require_board_eval(board);
    used.clear();
    mark_unique(used, hole);
    mark_unique(used, board);
    mark_unique(used, extra_dead);
}

bool big_o_nuts_from_idx(const int hole[5], const int* board, int board_n, std::uint64_t extra_mask) {
    DeckBitset used;
    used.mask = extra_mask;
    for (int i = 0; i < 5; ++i) {
        if (used.test(hole[i])) {
            throw std::invalid_argument("duplicate card");
        }
        used.set(hole[i]);
    }
    for (int i = 0; i < board_n; ++i) {
        if (used.test(board[i])) {
            throw std::invalid_argument("duplicate card");
        }
        used.set(board[i]);
    }
    const std::uint64_t hero_s = big_o_strength_from_idx(hole, board, board_n);
    const std::vector<int> pool = used.unused_indices();
    bool beaten = false;
    for_each_combo_indices(pool, 5, [&](const int* vil, int) {
        if (beaten) {
            return;
        }
        if (big_o_strength_from_idx(vil, board, board_n) > hero_s) {
            beaten = true;
        }
    });
    return !beaten;
}

double showdown_share(std::uint64_t hero, std::uint64_t villain) {
    if (hero > villain) {
        return 1.0;
    }
    if (hero < villain) {
        return 0.0;
    }
    return 0.5;
}

void sample_without_replace(std::vector<int>& pool, int take, std::mt19937& rng) {
    const int n = static_cast<int>(pool.size());
    for (int i = 0; i < take; ++i) {
        std::uniform_int_distribution<int> dist(i, n - 1);
        const int j = dist(rng);
        std::swap(pool[static_cast<std::size_t>(i)], pool[static_cast<std::size_t>(j)]);
    }
}

[[nodiscard]] std::uint64_t binom_n_5(int n) {
    if (n < 5) {
        return 0;
    }
    const std::uint64_t x = static_cast<std::uint64_t>(n);
    return x * (x - 1) * (x - 2) * (x - 3) * (x - 4) / 120;
}

}  // namespace

HandEvaluation evaluate_big_o_best_hand(const std::vector<Card>& hole, const std::vector<Card>& board) {
    require_hole5(hole);
    require_board_eval(board);
    DeckBitset used;
    mark_unique(used, hole);
    mark_unique(used, board);
    int h[5]{};
    int b[5]{};
    cards_to_idx(hole, h);
    cards_to_idx(board, b);
    return big_o_best_from_idx(h, b, static_cast<int>(board.size()));
}

std::uint64_t evaluate_big_o_hand_strength(const std::vector<Card>& hole,
                                           const std::vector<Card>& board) {
    return pack_hand_strength(evaluate_big_o_best_hand(hole, board));
}

double exact_hu_big_o_equity_vs_known(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                      const std::vector<Card>& board) {
    require_hole5(hero);
    require_hole5(villain);
    require_board_runout(board);
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, villain);
    mark_unique(used, board);
    int h[5]{};
    int v[5]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(villain, v);
    cards_to_idx(board, b);
    const int board_n = static_cast<int>(board.size());
    const int need = 5 - board_n;
    if (need == 0) {
        return showdown_share(big_o_strength_from_idx(h, b, 5), big_o_strength_from_idx(v, b, 5));
    }
    const std::vector<int> pool = used.unused_indices();
    double win = 0.0;
    double total = 0.0;
    for_each_combo_indices(pool, need, [&](const int* run, int run_k) {
        int full[5]{};
        for (int i = 0; i < board_n; ++i) {
            full[i] = b[i];
        }
        for (int i = 0; i < run_k; ++i) {
            full[board_n + i] = run[i];
        }
        win += showdown_share(big_o_strength_from_idx(h, full, 5), big_o_strength_from_idx(v, full, 5));
        total += 1.0;
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exactHuBigOEquityVsKnown: empty enumeration");
    }
    return win / total;
}

float simulate_big_o_equity_vs_random(const std::vector<Card>& hero, const std::vector<Card>& board,
                                      int trials, std::mt19937& rng) {
    require_hole5(hero);
    require_board_runout(board);
    if (trials <= 0) {
        return 0.0F;
    }
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, board);
    int h[5]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    const int board_n = static_cast<int>(board.size());
    const int need = 5 - board_n;
    const int take = 5 + need;
    const std::vector<int> live0 = used.unused_indices();
    if (static_cast<int>(live0.size()) < take) {
        throw std::invalid_argument("simulateBigOEquityVsRandom: not enough cards");
    }
    double win = 0.0;
    for (int t = 0; t < trials; ++t) {
        std::vector<int> pool = live0;
        sample_without_replace(pool, take, rng);
        const int* vil = pool.data();
        int full[5]{};
        for (int i = 0; i < board_n; ++i) {
            full[i] = b[i];
        }
        for (int i = 0; i < need; ++i) {
            full[board_n + i] = pool[static_cast<std::size_t>(5 + i)];
        }
        win += showdown_share(big_o_strength_from_idx(h, full, 5), big_o_strength_from_idx(vil, full, 5));
    }
    return static_cast<float>(win / static_cast<double>(trials));
}

float simulate_big_o_equity_vs_range(const std::vector<Card>& hero, const std::vector<Card>& board,
                                     const std::vector<BigORangeCombo>& range, int trials,
                                     std::mt19937& rng) {
    require_hole5(hero);
    require_board_runout(board);
    if (trials <= 0) {
        return 0.0F;
    }
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, board);
    std::vector<BigORangeCombo> live;
    live.reserve(range.size());
    double wsum = 0.0;
    for (const BigORangeCombo& c : range) {
        if (c.weight <= 0.0) {
            continue;
        }
        DeckBitset combo;
        bool ok = true;
        for (int i = 0; i < 5; ++i) {
            const int idx = c.cards[i];
            if (idx < 0 || idx > 51 || used.test(idx) || combo.test(idx)) {
                ok = false;
                break;
            }
            combo.set(idx);
        }
        if (!ok) {
            continue;
        }
        live.push_back(c);
        wsum += c.weight;
    }
    if (live.empty() || wsum <= 0.0) {
        throw std::invalid_argument("simulateBigOEquityVsRange: no live combos");
    }
    int h[5]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    const int board_n = static_cast<int>(board.size());
    const int need = 5 - board_n;
    std::vector<double> cdf(live.size());
    double acc = 0.0;
    for (std::size_t i = 0; i < live.size(); ++i) {
        acc += live[i].weight;
        cdf[i] = acc;
    }
    std::uniform_real_distribution<double> pick(0.0, wsum);
    double win = 0.0;
    for (int t = 0; t < trials; ++t) {
        const double u = pick(rng);
        const auto it = std::lower_bound(cdf.begin(), cdf.end(), u);
        std::size_t ci = static_cast<std::size_t>(it - cdf.begin());
        if (ci >= live.size()) {
            ci = live.size() - 1;
        }
        DeckBitset trial = used;
        for (int i = 0; i < 5; ++i) {
            trial.set(live[ci].cards[i]);
        }
        std::vector<int> pool = trial.unused_indices();
        if (static_cast<int>(pool.size()) < need) {
            throw std::invalid_argument("simulateBigOEquityVsRange: not enough cards");
        }
        if (need > 0) {
            sample_without_replace(pool, need, rng);
        }
        int full[5]{};
        for (int i = 0; i < board_n; ++i) {
            full[i] = b[i];
        }
        for (int i = 0; i < need; ++i) {
            full[board_n + i] = pool[static_cast<std::size_t>(i)];
        }
        win += showdown_share(big_o_strength_from_idx(h, full, 5),
                              big_o_strength_from_idx(live[ci].cards, full, 5));
    }
    return static_cast<float>(win / static_cast<double>(trials));
}

std::uint64_t big_o_combo_count(const std::vector<Card>& dead) {
    DeckBitset used;
    mark_unique(used, dead);
    return binom_n_5(52 - used.count());
}

bool big_o_nuts_on_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                         const std::vector<Card>& extra_dead) {
    DeckBitset used;
    split_dead(hero, board, extra_dead, used);
    int h[5]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    DeckBitset extra;
    extra.mark_cards(extra_dead);
    return big_o_nuts_from_idx(h, b, static_cast<int>(board.size()), extra.mask);
}

std::vector<double> big_o_multiway_equity_mc(const std::vector<std::vector<Card>>& holes,
                                             const std::vector<Card>& board, int trials,
                                             std::mt19937& rng) {
    const int n = static_cast<int>(holes.size());
    if (n < 3 || n > 4) {
        throw std::invalid_argument("bigOMultiwayEquityMc: need 3 or 4 hands");
    }
    require_board_runout(board);
    DeckBitset used;
    std::array<std::array<int, 5>, 4> hid{};
    for (int p = 0; p < n; ++p) {
        require_hole5(holes[static_cast<std::size_t>(p)]);
        mark_unique(used, holes[static_cast<std::size_t>(p)]);
        cards_to_idx(holes[static_cast<std::size_t>(p)], hid[static_cast<std::size_t>(p)].data());
    }
    mark_unique(used, board);
    int b[5]{};
    cards_to_idx(board, b);
    const int board_n = static_cast<int>(board.size());
    const int need = 5 - board_n;
    const std::vector<int> live0 = used.unused_indices();
    if (static_cast<int>(live0.size()) < need) {
        throw std::invalid_argument("bigOMultiwayEquityMc: not enough cards");
    }
    std::vector<double> eq(static_cast<std::size_t>(n), 0.0);
    if (trials <= 0) {
        return eq;
    }
    for (int t = 0; t < trials; ++t) {
        std::vector<int> pool = live0;
        if (need > 0) {
            sample_without_replace(pool, need, rng);
        }
        int full[5]{};
        for (int i = 0; i < board_n; ++i) {
            full[i] = b[i];
        }
        for (int i = 0; i < need; ++i) {
            full[board_n + i] = pool[static_cast<std::size_t>(i)];
        }
        std::uint64_t str[4]{};
        std::uint64_t best = 0;
        for (int p = 0; p < n; ++p) {
            str[p] = big_o_strength_from_idx(hid[static_cast<std::size_t>(p)].data(), full, 5);
            if (p == 0 || best < str[p]) {
                best = str[p];
            }
        }
        int tied = 0;
        for (int p = 0; p < n; ++p) {
            if (str[p] == best) {
                ++tied;
            }
        }
        const double share = 1.0 / static_cast<double>(tied);
        for (int p = 0; p < n; ++p) {
            if (str[p] == best) {
                eq[static_cast<std::size_t>(p)] += share;
            }
        }
    }
    for (double& x : eq) {
        x /= static_cast<double>(trials);
    }
    return eq;
}

}  // namespace poker
