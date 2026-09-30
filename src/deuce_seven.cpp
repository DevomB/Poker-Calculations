#include "poker/deuce_seven.hpp"

#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace poker {
namespace {

constexpr std::uint64_t kExactHuCap = 100000;

void mark_unique(DeckBitset& used, const std::vector<Card>& cards, const char* ctx) {
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (used.test(idx)) {
            throw std::invalid_argument(std::string(ctx) + ": duplicate card");
        }
        used.set(idx);
    }
}

void require_five(const std::vector<Card>& cards, const char* ctx) {
    if (cards.size() != 5) {
        throw std::invalid_argument(std::string(ctx) + ": need exactly 5 cards");
    }
    DeckBitset used;
    mark_unique(used, cards, ctx);
}

void cards_to_idx(const std::vector<Card>& cards, int* out) {
    for (std::size_t i = 0; i < cards.size(); ++i) {
        out[i] = deck_index_from_card(cards[i]);
    }
}

[[nodiscard]] std::uint64_t binom_u64(int n, int k) {
    if (k < 0 || n < 0 || k > n) {
        return 0;
    }
    if (k == 0 || k == n) {
        return 1;
    }
    if (k > n - k) {
        k = n - k;
    }
    std::uint64_t r = 1;
    for (int i = 1; i <= k; ++i) {
        r = r * static_cast<std::uint64_t>(n - k + i) / static_cast<std::uint64_t>(i);
    }
    return r;
}

int straight_high_no_wheel(const std::array<int, 13>& present) {
    for (int high = 12; high >= 4; --high) {
        bool ok = true;
        for (int d = 0; d < 5; ++d) {
            if (!present[static_cast<std::size_t>(high - d)]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return high;
        }
    }
    return -1;
}

HandEvaluation evaluate_rs(const std::uint8_t ranks[5], const std::uint8_t suits[5]) {
    HandEvaluation e{};
    std::array<int, 13> freq{};
    std::array<int, 4> suit_n{};
    for (int i = 0; i < 5; ++i) {
        freq[ranks[i]]++;
        suit_n[suits[i]]++;
    }
    const bool flush = suit_n[0] == 5 || suit_n[1] == 5 || suit_n[2] == 5 || suit_n[3] == 5;
    std::array<int, 13> present{};
    for (int r = 0; r < 13; ++r) {
        present[static_cast<std::size_t>(r)] = freq[static_cast<std::size_t>(r)] > 0 ? 1 : 0;
    }

    std::vector<std::pair<int, int>> groups;
    groups.reserve(5);
    for (int r = 12; r >= 0; --r) {
        const int c = freq[static_cast<std::size_t>(r)];
        if (c > 0) {
            groups.emplace_back(r, c);
        }
    }
    std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first > b.first;
    });

    const int sh = straight_high_no_wheel(present);
    if (flush && sh >= 0) {
        e.rank = (sh == 12) ? HandRank::RoyalFlush : HandRank::StraightFlush;
        e.kickers[0] = static_cast<std::uint8_t>(sh);
        return e;
    }
    if (!groups.empty() && groups[0].second == 4) {
        e.rank = HandRank::FourOfAKind;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (std::size_t i = 1; i < groups.size() && ki < 5; ++i) {
            for (int t = 0; t < groups[i].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(groups[i].first);
            }
        }
        return e;
    }
    if (groups.size() >= 2 && groups[0].second == 3 && groups[1].second == 2) {
        e.rank = HandRank::FullHouse;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        e.kickers[1] = static_cast<std::uint8_t>(groups[1].first);
        return e;
    }
    if (flush) {
        e.rank = HandRank::Flush;
        int ki = 0;
        for (const auto& g : groups) {
            for (int t = 0; t < g.second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(g.first);
            }
        }
        return e;
    }
    if (sh >= 0) {
        e.rank = HandRank::Straight;
        e.kickers[0] = static_cast<std::uint8_t>(sh);
        return e;
    }
    if (!groups.empty() && groups[0].second == 3) {
        e.rank = HandRank::ThreeOfAKind;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (std::size_t i = 1; i < groups.size() && ki < 5; ++i) {
            for (int t = 0; t < groups[i].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(groups[i].first);
            }
        }
        return e;
    }
    std::vector<int> pair_ranks;
    for (const auto& g : groups) {
        if (g.second == 2) {
            pair_ranks.push_back(g.first);
        }
    }
    if (pair_ranks.size() >= 2) {
        e.rank = HandRank::TwoPair;
        e.kickers[0] = static_cast<std::uint8_t>(pair_ranks[0]);
        e.kickers[1] = static_cast<std::uint8_t>(pair_ranks[1]);
        int ki = 2;
        for (const auto& g : groups) {
            if (g.second == 1) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(g.first);
            }
        }
        return e;
    }
    if (!groups.empty() && groups[0].second == 2) {
        e.rank = HandRank::OnePair;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (std::size_t i = 1; i < groups.size() && ki < 5; ++i) {
            for (int t = 0; t < groups[i].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(groups[i].first);
            }
        }
        return e;
    }
    e.rank = HandRank::HighCard;
    int ki = 0;
    for (const auto& g : groups) {
        for (int t = 0; t < g.second && ki < 5; ++t) {
            e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(g.first);
        }
    }
    return e;
}

void idx_to_rs(const int* idx, int n, std::uint8_t* ranks, std::uint8_t* suits) {
    for (int i = 0; i < n; ++i) {
        ranks[i] = static_cast<std::uint8_t>(idx[i] / 4);
        suits[i] = static_cast<std::uint8_t>(idx[i] % 4);
    }
}

std::uint64_t strength_idx(const int idx[5]) {
    std::uint8_t r[5]{};
    std::uint8_t s[5]{};
    idx_to_rs(idx, 5, r, s);
    return pack_hand_strength(evaluate_rs(r, s));
}

HandEvaluation eval_idx(const int idx[5]) {
    std::uint8_t r[5]{};
    std::uint8_t s[5]{};
    idx_to_rs(idx, 5, r, s);
    return evaluate_rs(r, s);
}

void assemble(const int* keep, int nkeep, const int* draw, int ndraw, int out[5]) {
    int t = 0;
    for (int i = 0; i < nkeep; ++i) {
        out[t++] = keep[i];
    }
    for (int i = 0; i < ndraw; ++i) {
        out[t++] = draw[i];
    }
}

bool is_nuts_eval(const HandEvaluation& e) {
    return e.rank == HandRank::HighCard && e.kickers[0] == 5 && e.kickers[1] == 3 && e.kickers[2] == 2 &&
           e.kickers[3] == 1 && e.kickers[4] == 0;
}

bool is_unpaired_eight(const HandEvaluation& e) {
    return e.rank == HandRank::HighCard && e.kickers[0] == 6;
}

bool is_smooth_unpaired(const HandEvaluation& e) {
    return e.rank == HandRank::HighCard && e.kickers[1] != static_cast<std::uint8_t>(e.kickers[0] - 1);
}

std::vector<int> live_pool(const DeckBitset& used) {
    return used.unused_indices();
}

int popcount_mask(int mask) {
    int n = 0;
    while (mask) {
        n += mask & 1;
        mask >>= 1;
    }
    return n;
}

void keep_from_mask(const int hand[5], int discard_mask, int* keep, int* nkeep) {
    *nkeep = 0;
    for (int i = 0; i < 5; ++i) {
        if ((discard_mask & (1 << i)) == 0) {
            keep[(*nkeep)++] = hand[i];
        }
    }
}

double showdown_share(std::uint64_t hero, std::uint64_t vil) {
    if (hero < vil) {
        return 1.0;
    }
    if (hero > vil) {
        return 0.0;
    }
    return 0.5;
}

double expected_strength(const int hand[5], int discard_mask, const std::vector<int>& pool) {
    const int k = popcount_mask(discard_mask);
    int keep[5]{};
    int nkeep = 0;
    keep_from_mask(hand, discard_mask, keep, &nkeep);
    const std::uint64_t ref = strength_idx(hand);
    if (k == 0) {
        return 0.5;
    }
    if (static_cast<int>(pool.size()) < k) {
        throw std::invalid_argument("deuce-seven: not enough cards to draw");
    }
    double sum = 0.0;
    std::uint64_t n = 0;
    for_each_combo_indices(pool, k, [&](const int* combo, int) {
        int five[5]{};
        assemble(keep, nkeep, combo, k, five);
        sum += showdown_share(strength_idx(five), ref);
        ++n;
    });
    return sum / static_cast<double>(n);
}

int best_mask_of_size(const int hand[5], int k, const std::vector<int>& pool, double* out_ev) {
    int best_mask = -1;
    double best_ev = -1.0;
    for (int mask = 0; mask < 32; ++mask) {
        if (popcount_mask(mask) != k) {
            continue;
        }
        const double ev = expected_strength(hand, mask, pool);
        if (best_mask < 0 || ev > best_ev || (ev == best_ev && mask < best_mask)) {
            best_ev = ev;
            best_mask = mask;
        }
    }
    if (out_ev) {
        *out_ev = best_ev;
    }
    return best_mask;
}

int cards_to_mask(const int hand[5], const std::vector<Card>& subset, const char* ctx) {
    int mask = 0;
    DeckBitset seen;
    for (const Card& c : subset) {
        const int id = deck_index_from_card(c);
        if (seen.test(id)) {
            throw std::invalid_argument(std::string(ctx) + ": duplicate discard/keep card");
        }
        seen.set(id);
        int found = -1;
        for (int i = 0; i < 5; ++i) {
            if (hand[i] == id) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            throw std::invalid_argument(std::string(ctx) + ": card not in hand");
        }
        mask |= (1 << found);
    }
    return mask;
}

int resolve_discard_mask(const int hand[5], const DeuceSevenDrawSpec& spec, const std::vector<int>& pool,
                         const char* ctx) {
    const int specified = (spec.has_discard_cards ? 1 : 0) + (spec.has_keep_cards ? 1 : 0) +
                          (spec.has_count ? 1 : 0);
    if (specified > 1) {
        throw std::invalid_argument(std::string(ctx) + ": give discard cards, keep cards, or a count");
    }
    if (spec.has_discard_cards) {
        return cards_to_mask(hand, spec.discard_cards, ctx);
    }
    if (spec.has_keep_cards) {
        const int keep_mask = cards_to_mask(hand, spec.keep_cards, ctx);
        return keep_mask ^ 0x1F;
    }
    int k = spec.has_count ? spec.discard_count : 0;
    if (k < 0 || k > 5) {
        throw std::invalid_argument(std::string(ctx) + ": discard count must be 0..5");
    }
    if (k == 0) {
        return 0;
    }
    const int mask = best_mask_of_size(hand, k, pool, nullptr);
    if (mask < 0) {
        throw std::invalid_argument(std::string(ctx) + ": no legal discard");
    }
    return mask;
}

template <typename Fn>
void for_each_replace(const std::vector<int>& pool, int k, Fn&& fn) {
    if (k == 0) {
        fn(nullptr, 0);
        return;
    }
    for_each_combo_indices(pool, k, std::forward<Fn>(fn));
}

}  // namespace

HandEvaluation evaluate_deuce_seven_five(const std::vector<Card>& five) {
    require_five(five, "evaluateDeuceSevenHand");
    int idx[5]{};
    cards_to_idx(five, idx);
    return eval_idx(idx);
}

std::uint64_t evaluate_deuce_seven_hand(const std::vector<Card>& five) {
    require_five(five, "evaluateDeuceSevenHand");
    int idx[5]{};
    cards_to_idx(five, idx);
    return strength_idx(idx);
}

const char* evaluate_deuce_seven_category(const std::vector<Card>& five) {
    const HandEvaluation e = evaluate_deuce_seven_five(five);
    switch (e.rank) {
        case HandRank::HighCard:
            if (is_nuts_eval(e)) {
                return "nuts";
            }
            if (e.kickers[0] <= 6) {
                return is_smooth_unpaired(e) ? "smooth" : "rough";
            }
            return "number";
        case HandRank::OnePair:
            return "paired";
        case HandRank::TwoPair:
            return "twoPair";
        case HandRank::ThreeOfAKind:
            return "trips";
        case HandRank::Straight:
            return "straight";
        case HandRank::Flush:
            return "flush";
        case HandRank::FullHouse:
            return "fullHouse";
        case HandRank::FourOfAKind:
            return "quads";
        case HandRank::StraightFlush:
        case HandRank::RoyalFlush:
            return "straightFlush";
        default:
            return "number";
    }
}

bool deuce_seven_is_pat(const std::vector<Card>& five, bool eight_pat) {
    const HandEvaluation e = evaluate_deuce_seven_five(five);
    if (e.rank != HandRank::HighCard) {
        return false;
    }
    const std::uint8_t cap = eight_pat ? 6 : 5;
    return e.kickers[0] <= cap;
}

bool deuce_seven_nuts_pat(const std::vector<Card>& five) {
    return is_nuts_eval(evaluate_deuce_seven_five(five));
}

double deuce_seven_draw_equity_vs_known(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                        const DeuceSevenDrawSpec& hero_draw, const DeuceSevenDrawSpec& villain_draw,
                                        const std::vector<Card>& extra_dead, int trials, std::mt19937& rng) {
    require_five(hero, "deuceSevenDrawEquityVsKnown hero");
    require_five(villain, "deuceSevenDrawEquityVsKnown villain");
    DeckBitset used;
    mark_unique(used, hero, "deuceSevenDrawEquityVsKnown");
    mark_unique(used, villain, "deuceSevenDrawEquityVsKnown");
    mark_unique(used, extra_dead, "deuceSevenDrawEquityVsKnown");
    const std::vector<int> pool = live_pool(used);
    int h[5]{};
    int v[5]{};
    cards_to_idx(hero, h);
    cards_to_idx(villain, v);
    const int hmask = resolve_discard_mask(h, hero_draw, pool, "hero");
    const int vmask = resolve_discard_mask(v, villain_draw, pool, "villain");
    const int hk = popcount_mask(hmask);
    const int vk = popcount_mask(vmask);
    int hkeep[5]{};
    int vkeep[5]{};
    int hn = 0;
    int vn = 0;
    keep_from_mask(h, hmask, hkeep, &hn);
    keep_from_mask(v, vmask, vkeep, &vn);
    const int n = static_cast<int>(pool.size());
    if (n < hk + vk) {
        throw std::invalid_argument("deuceSevenDrawEquityVsKnown: not enough cards to draw");
    }
    const std::uint64_t ways = (hk == 0 && vk == 0) ? 1 : binom_u64(n, hk) * binom_u64(n - hk, vk);

    auto play = [&](const int* hdraw, int, const int* vdraw, int) -> double {
        int hf[5]{};
        int vf[5]{};
        assemble(hkeep, hn, hdraw, hk, hf);
        assemble(vkeep, vn, vdraw, vk, vf);
        return showdown_share(strength_idx(hf), strength_idx(vf));
    };

    if (ways <= kExactHuCap) {
        double sum = 0.0;
        std::uint64_t count = 0;
        for_each_replace(pool, hk, [&](const int* hdraw, int) {
            std::vector<int> rest;
            rest.reserve(static_cast<std::size_t>(n - hk));
            DeckBitset taken;
            for (int i = 0; i < hk; ++i) {
                taken.set(hdraw[i]);
            }
            for (int id : pool) {
                if (!taken.test(id)) {
                    rest.push_back(id);
                }
            }
            for_each_replace(rest, vk, [&](const int* vdraw, int) {
                sum += play(hdraw, hk, vdraw, vk);
                ++count;
            });
        });
        return sum / static_cast<double>(count);
    }
    if (trials < 1) {
        throw std::invalid_argument("deuceSevenDrawEquityVsKnown: trials must be >= 1");
    }
    std::vector<int> live = pool;
    double sum = 0.0;
    for (int t = 0; t < trials; ++t) {
        std::shuffle(live.begin(), live.end(), rng);
        const int* hdraw = hk ? live.data() : nullptr;
        const int* vdraw = vk ? live.data() + hk : nullptr;
        sum += play(hdraw, hk, vdraw, vk);
    }
    return sum / static_cast<double>(trials);
}

DeuceSevenRoughSmooth deuce_seven_rough_vs_smooth(const std::vector<Card>& a, const std::vector<Card>& b) {
    require_five(a, "deuceSevenRoughVsSmooth");
    require_five(b, "deuceSevenRoughVsSmooth");
    DeckBitset used;
    mark_unique(used, a, "deuceSevenRoughVsSmooth");
    mark_unique(used, b, "deuceSevenRoughVsSmooth");
    int ia[5]{};
    int ib[5]{};
    cards_to_idx(a, ia);
    cards_to_idx(b, ib);
    const HandEvaluation ea = eval_idx(ia);
    const HandEvaluation eb = eval_idx(ib);
    if (!is_unpaired_eight(ea) || !is_unpaired_eight(eb)) {
        throw std::invalid_argument("deuceSevenRoughVsSmooth: both hands must be unpaired 8-high");
    }
    DeuceSevenRoughSmooth out;
    out.a_smooth = is_smooth_unpaired(ea);
    out.b_smooth = is_smooth_unpaired(eb);
    const std::uint64_t sa = pack_hand_strength(ea);
    const std::uint64_t sb = pack_hand_strength(eb);
    if (sa < sb) {
        out.cmp = -1;
    } else if (sa > sb) {
        out.cmp = 1;
    }
    return out;
}

std::vector<double> deuce_seven_multiway_showdown(const std::vector<std::vector<Card>>& hands) {
    if (hands.size() < 2 || hands.size() > 8) {
        throw std::invalid_argument("deuceSevenMultiwayShowdown: need 2..8 hands");
    }
    DeckBitset used;
    std::vector<std::uint64_t> str(hands.size());
    for (std::size_t i = 0; i < hands.size(); ++i) {
        require_five(hands[i], "deuceSevenMultiwayShowdown");
        mark_unique(used, hands[i], "deuceSevenMultiwayShowdown");
        int idx[5]{};
        cards_to_idx(hands[i], idx);
        str[i] = strength_idx(idx);
    }
    std::uint64_t best = str[0];
    for (std::size_t i = 1; i < str.size(); ++i) {
        if (str[i] < best) {
            best = str[i];
        }
    }
    int winners = 0;
    for (std::uint64_t s : str) {
        if (s == best) {
            ++winners;
        }
    }
    const double share = 1.0 / static_cast<double>(winners);
    std::vector<double> out(hands.size(), 0.0);
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == best) {
            out[i] = share;
        }
    }
    return out;
}

}  // namespace poker
