#include "poker/omaha_hi_lo.hpp"

#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/omaha.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace poker {
namespace {

constexpr int kHolePairs[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};

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

void idx_to_ranks(const int* idx, int n, std::uint8_t* ranks) {
    for (int i = 0; i < n; ++i) {
        ranks[i] = static_cast<std::uint8_t>(idx[i] / 4);
    }
}

void idx_to_rs(const int* idx, int n, std::uint8_t* ranks, std::uint8_t* suits) {
    for (int i = 0; i < n; ++i) {
        ranks[i] = static_cast<std::uint8_t>(idx[i] / 4);
        suits[i] = static_cast<std::uint8_t>(idx[i] % 4);
    }
}

/// Ace → 1; 2–8 → 2–8; 9+ → 0 (not a low rank).
[[nodiscard]] std::uint8_t plo8_low_value(std::uint8_t rank) {
    if (rank == 12) {
        return 1;
    }
    if (rank <= 6) {
        return static_cast<std::uint8_t>(rank + 2);
    }
    return 0;
}

[[nodiscard]] bool pack_five_card_low(const std::uint8_t ranks[5], std::uint8_t sorted[5],
                                      std::uint32_t* key) {
    std::uint8_t vals[5]{};
    unsigned seen = 0;
    for (int i = 0; i < 5; ++i) {
        const std::uint8_t v = plo8_low_value(ranks[i]);
        if (v == 0) {
            return false;
        }
        const unsigned bit = 1u << v;
        if ((seen & bit) != 0) {
            return false;
        }
        seen |= bit;
        vals[i] = v;
    }
    std::sort(vals, vals + 5, std::greater<std::uint8_t>());
    for (int i = 0; i < 5; ++i) {
        sorted[i] = vals[i];
    }
    *key = (static_cast<std::uint32_t>(vals[0]) << 16) | (static_cast<std::uint32_t>(vals[1]) << 12) |
           (static_cast<std::uint32_t>(vals[2]) << 8) | (static_cast<std::uint32_t>(vals[3]) << 4) |
           static_cast<std::uint32_t>(vals[4]);
    return true;
}

OmahaLoHand omaha_lo_from_idx(const int hole[4], const int* board, int board_n) {
    std::uint8_t hr[4]{};
    std::uint8_t br[5]{};
    idx_to_ranks(hole, 4, hr);
    idx_to_ranks(board, board_n, br);

    OmahaLoHand best{};
    for (int a = 0; a < board_n - 2; ++a) {
        for (int b = a + 1; b < board_n - 1; ++b) {
            for (int c = b + 1; c < board_n; ++c) {
                for (const auto& pair : kHolePairs) {
                    const std::uint8_t ranks[5] = {hr[pair[0]], hr[pair[1]], br[a], br[b], br[c]};
                    std::uint8_t sorted[5]{};
                    std::uint32_t key = kOmahaNoLow;
                    if (!pack_five_card_low(ranks, sorted, &key)) {
                        continue;
                    }
                    if (!best.qualifies || key < best.key) {
                        best.qualifies = true;
                        best.key = key;
                        for (int i = 0; i < 5; ++i) {
                            best.ranks[static_cast<std::size_t>(i)] = sorted[i];
                        }
                    }
                }
            }
        }
    }
    return best;
}

std::uint64_t omaha_hi_from_idx(const int hole[4], const int* board, int board_n) {
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
    return pack_hand_strength(best);
}

[[nodiscard]] bool board_allows_low(const int* board, int board_n) {
    std::uint8_t br[5]{};
    idx_to_ranks(board, board_n, br);
    for (int a = 0; a < board_n - 2; ++a) {
        for (int b = a + 1; b < board_n - 1; ++b) {
            for (int c = b + 1; c < board_n; ++c) {
                const std::uint8_t va = plo8_low_value(br[a]);
                const std::uint8_t vb = plo8_low_value(br[b]);
                const std::uint8_t vc = plo8_low_value(br[c]);
                if (va == 0 || vb == 0 || vc == 0) {
                    continue;
                }
                if (va != vb && va != vc && vb != vc) {
                    return true;
                }
            }
        }
    }
    return false;
}

double hi_share(std::uint64_t hero, std::uint64_t villain) {
    if (hero > villain) {
        return 1.0;
    }
    if (hero < villain) {
        return 0.0;
    }
    return 0.5;
}

double lo_share(std::uint32_t hero, std::uint32_t villain) {
    const bool hq = hero != kOmahaNoLow;
    const bool vq = villain != kOmahaNoLow;
    if (!hq && !vq) {
        return 0.0;
    }
    if (!hq) {
        return 0.0;
    }
    if (!vq) {
        return 1.0;
    }
    if (hero < villain) {
        return 1.0;
    }
    if (hero > villain) {
        return 0.0;
    }
    return 0.5;
}

struct HuDelta {
    double hi{0.0};
    double lo{0.0};
    double pot{0.0};
    double scoop{0.0};
    double quarter{0.0};
};

HuDelta hu_delta(std::uint64_t hero_hi, std::uint32_t hero_lo, std::uint64_t vil_hi,
                 std::uint32_t vil_lo) {
    HuDelta d{};
    d.hi = hi_share(hero_hi, vil_hi);
    const bool any_lo = hero_lo != kOmahaNoLow || vil_lo != kOmahaNoLow;
    if (!any_lo) {
        d.pot = d.hi;
        d.scoop = hero_hi > vil_hi ? 1.0 : 0.0;
        return d;
    }
    d.lo = lo_share(hero_lo, vil_lo);
    d.pot = 0.5 * d.hi + 0.5 * d.lo;
    const bool hi_win = hero_hi > vil_hi;
    const bool hi_tie = hero_hi == vil_hi;
    const bool lo_win = d.lo == 1.0;
    const bool lo_tie = d.lo == 0.5;
    d.scoop = (hi_win && lo_win) ? 1.0 : 0.0;
    d.quarter = ((hi_tie && !lo_tie) || (lo_tie && !hi_tie)) ? 1.0 : 0.0;
    return d;
}

void sample_without_replace(std::vector<int>& pool, int take, std::mt19937& rng) {
    const int n = static_cast<int>(pool.size());
    for (int i = 0; i < take; ++i) {
        std::uniform_int_distribution<int> dist(i, n - 1);
        const int j = dist(rng);
        std::swap(pool[static_cast<std::size_t>(i)], pool[static_cast<std::size_t>(j)]);
    }
}

void fill_board(int* full, const int* partial, int board_n, const int* run, int need) {
    for (int i = 0; i < board_n; ++i) {
        full[i] = partial[i];
    }
    for (int i = 0; i < need; ++i) {
        full[board_n + i] = run[i];
    }
}

OmahaHiLoEquity accumulate_exact(const int h[4], const int v[4], const int* b, int board_n,
                                 const std::vector<int>& pool, int need) {
    OmahaHiLoEquity out{};
    double total = 0.0;
    auto add = [&](const int* full) {
        const HuDelta d = hu_delta(omaha_hi_from_idx(h, full, 5), omaha_lo_from_idx(h, full, 5).key,
                                   omaha_hi_from_idx(v, full, 5), omaha_lo_from_idx(v, full, 5).key);
        out.hi_equity += d.hi;
        out.lo_equity += d.lo;
        out.scoop_equity += d.scoop;
        out.quarter_rate += d.quarter;
        out.pot_share += d.pot;
        total += 1.0;
    };
    if (need == 0) {
        int full[5]{};
        fill_board(full, b, 5, nullptr, 0);
        add(full);
    } else {
        for_each_combo_indices(pool, need, [&](const int* run, int) {
            int full[5]{};
            fill_board(full, b, board_n, run, need);
            add(full);
        });
    }
    if (total <= 0.0) {
        throw std::invalid_argument("exactHuOmahaHiLoEquity: empty enumeration");
    }
    out.hi_equity /= total;
    out.lo_equity /= total;
    out.scoop_equity /= total;
    out.quarter_rate /= total;
    out.pot_share /= total;
    return out;
}

OmahaHiLoEquity simulate_known(const int h[4], const int v[4], const int* b, int board_n,
                               const std::vector<int>& live0, int need, int trials, std::mt19937& rng) {
    OmahaHiLoEquity out{};
    if (trials <= 0) {
        return out;
    }
    for (int t = 0; t < trials; ++t) {
        std::vector<int> pool = live0;
        if (need > 0) {
            sample_without_replace(pool, need, rng);
        }
        int full[5]{};
        fill_board(full, b, board_n, pool.data(), need);
        const HuDelta d = hu_delta(omaha_hi_from_idx(h, full, 5), omaha_lo_from_idx(h, full, 5).key,
                                   omaha_hi_from_idx(v, full, 5), omaha_lo_from_idx(v, full, 5).key);
        out.hi_equity += d.hi;
        out.lo_equity += d.lo;
        out.scoop_equity += d.scoop;
        out.quarter_rate += d.quarter;
        out.pot_share += d.pot;
    }
    const double n = static_cast<double>(trials);
    out.hi_equity /= n;
    out.lo_equity /= n;
    out.scoop_equity /= n;
    out.quarter_rate /= n;
    out.pot_share /= n;
    return out;
}

}  // namespace

OmahaLoHand evaluate_omaha_lo_hand(const std::vector<Card>& hole, const std::vector<Card>& board) {
    require_hole4(hole);
    require_board_eval(board);
    DeckBitset used;
    mark_unique(used, hole);
    mark_unique(used, board);
    int h[4]{};
    int b[5]{};
    cards_to_idx(hole, h);
    cards_to_idx(board, b);
    return omaha_lo_from_idx(h, b, static_cast<int>(board.size()));
}

bool omaha_lo_qualifies(const std::vector<Card>& hole, const std::vector<Card>& board) {
    return evaluate_omaha_lo_hand(hole, board).qualifies;
}

OmahaHiLoHands evaluate_omaha_hi_lo(const std::vector<Card>& hole, const std::vector<Card>& board) {
    OmahaHiLoHands out{};
    out.hi = evaluate_omaha_best_hand(hole, board);
    out.lo = evaluate_omaha_lo_hand(hole, board);
    return out;
}

OmahaHiLoEquity exact_hu_omaha_hi_lo_equity(const std::vector<Card>& hero, const std::vector<Card>& villain,
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
    return accumulate_exact(h, v, b, board_n, used.unused_indices(), need);
}

OmahaHiLoEquity simulate_omaha_hi_lo_equity(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                            const std::vector<Card>& board, int trials, std::mt19937& rng) {
    require_hole4(hero);
    require_board_runout(board);
    if (trials <= 0) {
        return {};
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
    const bool vs_random = villain.empty();
    if (!vs_random) {
        require_hole4(villain);
        mark_unique(used, villain);
        int v[4]{};
        cards_to_idx(villain, v);
        const std::vector<int> live0 = used.unused_indices();
        if (static_cast<int>(live0.size()) < need) {
            throw std::invalid_argument("simulateOmahaHiLoEquity: not enough cards");
        }
        return simulate_known(h, v, b, board_n, live0, need, trials, rng);
    }
    const int take = 4 + need;
    const std::vector<int> live0 = used.unused_indices();
    if (static_cast<int>(live0.size()) < take) {
        throw std::invalid_argument("simulateOmahaHiLoEquity: not enough cards");
    }
    OmahaHiLoEquity out{};
    for (int t = 0; t < trials; ++t) {
        std::vector<int> pool = live0;
        sample_without_replace(pool, take, rng);
        const int* vil = pool.data();
        int full[5]{};
        fill_board(full, b, board_n, pool.data() + 4, need);
        const HuDelta d = hu_delta(omaha_hi_from_idx(h, full, 5), omaha_lo_from_idx(h, full, 5).key,
                                   omaha_hi_from_idx(vil, full, 5), omaha_lo_from_idx(vil, full, 5).key);
        out.hi_equity += d.hi;
        out.lo_equity += d.lo;
        out.scoop_equity += d.scoop;
        out.quarter_rate += d.quarter;
        out.pot_share += d.pot;
    }
    const double n = static_cast<double>(trials);
    out.hi_equity /= n;
    out.lo_equity /= n;
    out.scoop_equity /= n;
    out.quarter_rate /= n;
    out.pot_share /= n;
    return out;
}

double omaha_scoop_probability_mc(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                  const std::vector<Card>& board, int trials, std::mt19937& rng) {
    return simulate_omaha_hi_lo_equity(hero, villain, board, trials, rng).scoop_equity;
}

double omaha_quarter_probability_mc(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                    const std::vector<Card>& board, int trials, std::mt19937& rng) {
    return simulate_omaha_hi_lo_equity(hero, villain, board, trials, rng).quarter_rate;
}

bool omaha_lo_nuts_on_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                            const std::vector<Card>& extra_dead) {
    require_hole4(hero);
    require_board_eval(board);
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, board);
    mark_unique(used, extra_dead);
    int h[4]{};
    int b[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(board, b);
    const int bn = static_cast<int>(board.size());
    const OmahaLoHand hero_lo = omaha_lo_from_idx(h, b, bn);
    if (!hero_lo.qualifies) {
        return false;
    }
    DeckBitset block;
    block.mark_cards(hero);  // villain holdings cannot contain hero's cards
    block.mark_cards(board);
    block.mark_cards(extra_dead);
    bool beaten = false;
    for_each_combo_indices(block.unused_indices(), 4, [&](const int* vil, int) {
        if (beaten) {
            return;
        }
        const OmahaLoHand v = omaha_lo_from_idx(vil, b, bn);
        if (v.qualifies && v.key < hero_lo.key) {
            beaten = true;
        }
    });
    return !beaten;
}

OmahaHiLoNuttedness omaha_hi_lo_nuttedness(const std::vector<Card>& hero, const std::vector<Card>& board,
                                           const std::vector<Card>& extra_dead) {
    require_hole4(hero);
    require_board_eval(board);
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, board);
    mark_unique(used, extra_dead);
    int b[5]{};
    cards_to_idx(board, b);
    OmahaHiLoNuttedness out{};
    out.hi_nuts = omaha_nuts_on_board(hero, board, extra_dead);
    out.lo_nuts = omaha_lo_nuts_on_board(hero, board, extra_dead);
    out.scoop_nuts = out.hi_nuts && (out.lo_nuts || !board_allows_low(b, static_cast<int>(board.size())));
    return out;
}

std::vector<double> omaha_hi_lo_multiway_mc(const std::vector<std::vector<Card>>& holes,
                                            const std::vector<Card>& board, int trials,
                                            std::mt19937& rng) {
    if (holes.size() != 3) {
        throw std::invalid_argument("omahaHiLoMultiwayMc: need exactly 3 hands");
    }
    require_board_runout(board);
    DeckBitset used;
    std::array<std::array<int, 4>, 3> hid{};
    for (int p = 0; p < 3; ++p) {
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
        throw std::invalid_argument("omahaHiLoMultiwayMc: not enough cards");
    }
    std::vector<double> eq(3, 0.0);
    if (trials <= 0) {
        return eq;
    }
    for (int t = 0; t < trials; ++t) {
        std::vector<int> pool = live0;
        if (need > 0) {
            sample_without_replace(pool, need, rng);
        }
        int full[5]{};
        fill_board(full, b, board_n, pool.data(), need);
        std::uint64_t hi[3]{};
        std::uint32_t lo[3]{};
        std::uint64_t best_hi = 0;
        std::uint32_t best_lo = kOmahaNoLow;
        bool any_lo = false;
        for (int p = 0; p < 3; ++p) {
            hi[p] = omaha_hi_from_idx(hid[static_cast<std::size_t>(p)].data(), full, 5);
            lo[p] = omaha_lo_from_idx(hid[static_cast<std::size_t>(p)].data(), full, 5).key;
            if (p == 0 || best_hi < hi[p]) {
                best_hi = hi[p];
            }
            if (lo[p] != kOmahaNoLow) {
                any_lo = true;
                if (best_lo == kOmahaNoLow || lo[p] < best_lo) {
                    best_lo = lo[p];
                }
            }
        }
        int n_hi = 0;
        int n_lo = 0;
        for (int p = 0; p < 3; ++p) {
            if (hi[p] == best_hi) {
                ++n_hi;
            }
            if (any_lo && lo[p] == best_lo) {
                ++n_lo;
            }
        }
        for (int p = 0; p < 3; ++p) {
            if (!any_lo) {
                if (hi[p] == best_hi) {
                    eq[static_cast<std::size_t>(p)] += 1.0 / static_cast<double>(n_hi);
                }
            } else {
                if (hi[p] == best_hi) {
                    eq[static_cast<std::size_t>(p)] += 0.5 / static_cast<double>(n_hi);
                }
                if (lo[p] == best_lo) {
                    eq[static_cast<std::size_t>(p)] += 0.5 / static_cast<double>(n_lo);
                }
            }
        }
    }
    for (double& x : eq) {
        x /= static_cast<double>(trials);
    }
    return eq;
}

}  // namespace poker
