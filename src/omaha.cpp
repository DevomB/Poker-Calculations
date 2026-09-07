#include "poker/omaha.hpp"

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

constexpr int kHolePairs[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};

[[nodiscard]] bool is_straight_category(HandRank r) {
    return r == HandRank::Straight || r == HandRank::StraightFlush || r == HandRank::RoyalFlush;
}

void mark_unique(DeckBitset& used, const std::vector<Card>& cards) {
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (used.test(idx)) {
            throw std::invalid_argument("duplicate card");
        }
        used.set(idx);
    }
}

void require_hole4(const std::vector<Card>& hole) {
    if (hole.size() != 4) {
        throw std::invalid_argument("omaha hole must be exactly 4 cards");
    }
}

void require_board_eval(const std::vector<Card>& board) {
    if (board.size() < 3 || board.size() > 5) {
        throw std::invalid_argument("omaha evaluation needs 3..5 board cards");
    }
}

void require_board_runout(const std::vector<Card>& board) {
    if (board.size() > 5) {
        throw std::invalid_argument("omaha board must have at most 5 cards");
    }
}

void cards_to_idx(const std::vector<Card>& cards, int* out) {
    for (std::size_t i = 0; i < cards.size(); ++i) {
        out[i] = deck_index_from_card(cards[i]);
    }
}

void idx_to_rs(const int* idx, int n, std::uint8_t* ranks, std::uint8_t* suits) {
    for (int i = 0; i < n; ++i) {
        ranks[i] = static_cast<std::uint8_t>(idx[i] / 4);
        suits[i] = static_cast<std::uint8_t>(idx[i] % 4);
    }
}

HandEvaluation omaha_best_from_idx(const int hole[4], const int* board, int board_n) {
    std::uint8_t hr[4]{};
    std::uint8_t hs[4]{};
    std::uint8_t br[5]{};
    std::uint8_t bs[5]{};
    idx_to_rs(hole, 4, hr, hs);
    idx_to_rs(board, board_n, br, bs);

    HandEvaluation best{};
    bool init = false;
    for (int a = 0; a < board_n - 2; ++a) {
        for (int b = a + 1; b < board_n - 1; ++b) {
            for (int c = b + 1; c < board_n; ++c) {
                for (const auto& pair : kHolePairs) {
                    const std::uint8_t ranks[5] = {hr[pair[0]], hr[pair[1]], br[a], br[b], br[c]};
                    const std::uint8_t suits[5] = {hs[pair[0]], hs[pair[1]], bs[a], bs[b], bs[c]};
                    const HandEvaluation e = evaluate_five_cards_fast(ranks, suits);
                    if (!init || best < e) {
                        best = e;
                        init = true;
                    }
                }
            }
        }
    }
    return best;
}

std::uint64_t omaha_strength_from_idx(const int hole[4], const int* board, int board_n) {
    return pack_hand_strength(omaha_best_from_idx(hole, board, board_n));
}

void split_dead(const std::vector<Card>& hole, const std::vector<Card>& board,
                const std::vector<Card>& extra_dead, DeckBitset& used) {
    require_hole4(hole);
    require_board_eval(board);
    used.clear();
    mark_unique(used, hole);
    mark_unique(used, board);
    mark_unique(used, extra_dead);
}

bool omaha_nuts_from_idx(const int hole[4], const int* board, int board_n, std::uint64_t extra_mask) {
    DeckBitset used;
    used.mask = extra_mask;
    for (int i = 0; i < 4; ++i) {
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
    const std::uint64_t hero_s = omaha_strength_from_idx(hole, board, board_n);
    const std::vector<int> pool = used.unused_indices();
    bool beaten = false;
    for_each_combo_indices(pool, 4, [&](const int* vil, int) {
        if (beaten) {
            return;
        }
        if (omaha_strength_from_idx(vil, board, board_n) > hero_s) {
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

[[nodiscard]] std::uint64_t binom_n_4(int n) {
    if (n < 4) {
        return 0;
    }
    const std::uint64_t x = static_cast<std::uint64_t>(n);
    return x * (x - 1) * (x - 2) * (x - 3) / 24;
}

}  // namespace

HandEvaluation evaluate_omaha_best_hand(const std::vector<Card>& hole, const std::vector<Card>& board) {
    require_hole4(hole);
    require_board_eval(board);
    DeckBitset used;
    mark_unique(used, hole);
    mark_unique(used, board);
    int h[4]{};
    int b[5]{};
    cards_to_idx(hole, h);
    cards_to_idx(board, b);
    return omaha_best_from_idx(h, b, static_cast<int>(board.size()));
}

std::uint64_t evaluate_omaha_hand_strength(const std::vector<Card>& hole,
                                           const std::vector<Card>& board) {
    return pack_hand_strength(evaluate_omaha_best_hand(hole, board));
}

double exact_hu_omaha_equity_vs_known(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                      const std::vector<Card>& board) {
    require_hole4(hero);
    require_hole4(villain);
    require_board_runout(board);
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, villain);
    mark_unique(used, board);
    int h[4]{};
    int v[4]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(villain, v);
    cards_to_idx(board, b);
    const int board_n = static_cast<int>(board.size());
    const int need = 5 - board_n;
    if (need == 0) {
        return showdown_share(omaha_strength_from_idx(h, b, 5), omaha_strength_from_idx(v, b, 5));
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
        win += showdown_share(omaha_strength_from_idx(h, full, 5), omaha_strength_from_idx(v, full, 5));
        total += 1.0;
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exactHuOmahaEquityVsKnown: empty enumeration");
    }
    return win / total;
}

float simulate_omaha_equity_vs_random(const std::vector<Card>& hero, const std::vector<Card>& board,
                                      int trials, std::mt19937& rng) {
    require_hole4(hero);
    require_board_runout(board);
    if (trials <= 0) {
        return 0.0F;
    }
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, board);
    int h[4]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    const int board_n = static_cast<int>(board.size());
    const int need = 5 - board_n;
    const int take = 4 + need;
    const std::vector<int> live0 = used.unused_indices();
    if (static_cast<int>(live0.size()) < take) {
        throw std::invalid_argument("simulateOmahaEquityVsRandom: not enough cards");
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
            full[board_n + i] = pool[static_cast<std::size_t>(4 + i)];
        }
        win += showdown_share(omaha_strength_from_idx(h, full, 5), omaha_strength_from_idx(vil, full, 5));
    }
    return static_cast<float>(win / static_cast<double>(trials));
}

float simulate_omaha_equity_vs_range(const std::vector<Card>& hero, const std::vector<Card>& board,
                                     const std::vector<OmahaRangeCombo>& range, int trials,
                                     std::mt19937& rng) {
    require_hole4(hero);
    require_board_runout(board);
    if (trials <= 0) {
        return 0.0F;
    }
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, board);
    std::vector<OmahaRangeCombo> live;
    live.reserve(range.size());
    double wsum = 0.0;
    for (const OmahaRangeCombo& c : range) {
        if (c.weight <= 0.0) {
            continue;
        }
        DeckBitset combo;
        bool ok = true;
        for (int i = 0; i < 4; ++i) {
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
        throw std::invalid_argument("simulateOmahaEquityVsRange: no live combos");
    }
    int h[4]{};
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
        for (int i = 0; i < 4; ++i) {
            trial.set(live[ci].cards[i]);
        }
        std::vector<int> pool = trial.unused_indices();
        if (static_cast<int>(pool.size()) < need) {
            throw std::invalid_argument("simulateOmahaEquityVsRange: not enough cards");
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
        win += showdown_share(omaha_strength_from_idx(h, full, 5),
                              omaha_strength_from_idx(live[ci].cards, full, 5));
    }
    return static_cast<float>(win / static_cast<double>(trials));
}

std::uint64_t omaha_combo_count(const std::vector<Card>& dead) {
    DeckBitset used;
    mark_unique(used, dead);
    return binom_n_4(52 - used.count());
}

bool omaha_nuts_on_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                         const std::vector<Card>& extra_dead) {
    DeckBitset used;
    split_dead(hero, board, extra_dead, used);
    int h[4]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    DeckBitset extra;
    extra.mark_cards(extra_dead);
    return omaha_nuts_from_idx(h, b, static_cast<int>(board.size()), extra.mask);
}

OmahaWrapDrawOuts omaha_wrap_draw_outs(const std::vector<Card>& hero, const std::vector<Card>& flop) {
    require_hole4(hero);
    if (flop.size() != 3) {
        throw std::invalid_argument("omahaWrapDrawOuts: flop must be 3 cards");
    }
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, flop);
    int h[4]{};
    int f[3]{};
    cards_to_idx(hero, h);
    cards_to_idx(flop, f);
    OmahaWrapDrawOuts out{};
    for (int x : used.unused_indices()) {
        const int board4[4] = {f[0], f[1], f[2], x};
        const HandEvaluation e = omaha_best_from_idx(h, board4, 4);
        if (!is_straight_category(e.rank)) {
            continue;
        }
        ++out.outs;
        if (omaha_nuts_from_idx(h, board4, 4, 0)) {
            ++out.nut_outs;
        }
    }
    return out;
}

double omaha_nuttedness_score(const std::vector<Card>& hero, const std::vector<Card>& board,
                              const std::vector<Card>& extra_dead) {
    require_hole4(hero);
    require_board_eval(board);
    DeckBitset hero_used;
    mark_unique(hero_used, hero);
    DeckBitset block;
    mark_unique(block, board);
    mark_unique(block, extra_dead);
    int h[4]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    for (int i = 0; i < 4; ++i) {
        if (block.test(h[i])) {
            throw std::invalid_argument("duplicate card");
        }
    }
    const std::uint64_t hero_s = omaha_strength_from_idx(h, b, static_cast<int>(board.size()));
    const std::vector<int> pool = block.unused_indices();
    std::uint64_t better = 0;
    std::uint64_t total = 0;
    const int bn = static_cast<int>(board.size());
    for_each_combo_indices(pool, 4, [&](const int* combo, int) {
        ++total;
        if (omaha_strength_from_idx(combo, b, bn) > hero_s) {
            ++better;
        }
    });
    if (total <= 1) {
        return 1.0;
    }
    return 1.0 - static_cast<double>(better) / static_cast<double>(total - 1);
}

std::vector<double> omaha_multiway_equity_mc(const std::vector<std::vector<Card>>& holes,
                                             const std::vector<Card>& board, int trials,
                                             std::mt19937& rng) {
    const int n = static_cast<int>(holes.size());
    if (n < 3 || n > 4) {
        throw std::invalid_argument("omahaMultiwayEquityMc: need 3 or 4 hands");
    }
    require_board_runout(board);
    DeckBitset used;
    std::array<std::array<int, 4>, 4> hid{};
    for (int p = 0; p < n; ++p) {
        require_hole4(holes[static_cast<std::size_t>(p)]);
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
        throw std::invalid_argument("omahaMultiwayEquityMc: not enough cards");
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
            str[p] = omaha_strength_from_idx(hid[static_cast<std::size_t>(p)].data(), full, 5);
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
