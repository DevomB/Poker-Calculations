#include "poker/stud_razz.hpp"

#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>

namespace poker {
namespace {

constexpr int kStudMin = 3;
constexpr int kStudMax = 7;
constexpr std::uint64_t kMaxExactDeals = 2000000;

void mark_unique(DeckBitset& used, const std::vector<Card>& cards) {
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (used.test(idx)) {
            throw std::invalid_argument("duplicate card");
        }
        used.set(idx);
    }
}

void require_stud_len(const std::vector<Card>& cards, const char* ctx) {
    const int n = static_cast<int>(cards.size());
    if (n < kStudMin || n > kStudMax) {
        throw std::invalid_argument(std::string(ctx) + ": need 3..7 cards");
    }
}

[[nodiscard]] int ace_low(int holdem_rank) {
    return holdem_rank == 12 ? 0 : holdem_rank + 1;
}

[[nodiscard]] std::uint64_t binom(int n, int k) {
    if (k < 0 || k > n) {
        return 0;
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

void sample_without_replace(std::vector<int>& pool, int take, std::mt19937& rng) {
    const int n = static_cast<int>(pool.size());
    for (int i = 0; i < take; ++i) {
        std::uniform_int_distribution<int> dist(i, n - 1);
        const int j = dist(rng);
        std::swap(pool[static_cast<std::size_t>(i)], pool[static_cast<std::size_t>(j)]);
    }
}

[[nodiscard]] int pair_tier(HandRank r) {
    switch (r) {
        case HandRank::HighCard:
            return 0;
        case HandRank::OnePair:
            return 1;
        case HandRank::TwoPair:
            return 2;
        case HandRank::ThreeOfAKind:
            return 3;
        case HandRank::FullHouse:
            return 4;
        case HandRank::FourOfAKind:
            return 5;
        default:
            return 0;
    }
}

RazzEvaluation evaluate_razz_five(const Card* five, int n) {
    RazzEvaluation e{};
    std::array<int, 13> freq{};
    std::array<int, 5> live_al{};
    int live = 0;
    for (int i = 0; i < n; ++i) {
        const int r = five[i].rank();
        freq[static_cast<std::size_t>(r)]++;
        live_al[static_cast<std::size_t>(live++)] = ace_low(r);
    }

    int quads = -1;
    int trips = -1;
    int pairs[2] = {-1, -1};
    int n_pairs = 0;
    for (int r = 0; r < 13; ++r) {
        const int f = freq[static_cast<std::size_t>(r)];
        if (f >= 4) {
            quads = r;
        } else if (f == 3) {
            trips = r;
        } else if (f == 2) {
            if (n_pairs < 2) {
                pairs[n_pairs++] = r;
            }
        }
    }

    if (n_pairs == 2 && ace_low(pairs[0]) > ace_low(pairs[1])) {
        std::swap(pairs[0], pairs[1]);
    }

    if (quads >= 0) {
        e.rank = HandRank::FourOfAKind;
    } else if (trips >= 0 && n_pairs >= 1) {
        e.rank = HandRank::FullHouse;
    } else if (trips >= 0) {
        e.rank = HandRank::ThreeOfAKind;
    } else if (n_pairs >= 2) {
        e.rank = HandRank::TwoPair;
    } else if (n_pairs == 1) {
        e.rank = HandRank::OnePair;
    } else {
        e.rank = HandRank::HighCard;
    }

    // Incomplete hands: ghost 13s sit as the *high* cards so a 4-card 4-high is not
    // better than a made wheel. Same-street 3-card vs 3-card still compares the live ranks.
    auto write_rest = [&](int start, std::array<int, 5> rest, int rn) {
        std::sort(rest.begin(), rest.begin() + rn, std::greater<int>());
        // One ghost per missing card, never for a complete five-card hand.
        const int pads = std::max(0, 5 - n);
        int ki = start;
        for (int i = 0; i < pads; ++i) {
            e.kickers[static_cast<std::size_t>(ki++)] = 13;
        }
        for (int i = 0; i < rn; ++i) {
            e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(rest[static_cast<std::size_t>(i)]);
        }
    };

    if (e.rank == HandRank::FourOfAKind) {
        e.kickers[0] = static_cast<std::uint8_t>(ace_low(quads));
        std::array<int, 5> rest{};
        int rn = 0;
        for (int i = 0; i < live; ++i) {
            if (live_al[static_cast<std::size_t>(i)] != ace_low(quads)) {
                rest[static_cast<std::size_t>(rn++)] = live_al[static_cast<std::size_t>(i)];
            }
        }
        write_rest(1, rest, rn);
        return e;
    }
    if (e.rank == HandRank::FullHouse) {
        e.kickers[0] = static_cast<std::uint8_t>(ace_low(trips));
        e.kickers[1] = static_cast<std::uint8_t>(ace_low(pairs[0]));
        e.kickers[2] = 13;
        e.kickers[3] = 13;
        e.kickers[4] = 13;
        return e;
    }
    if (e.rank == HandRank::ThreeOfAKind) {
        e.kickers[0] = static_cast<std::uint8_t>(ace_low(trips));
        std::array<int, 5> rest{};
        int rn = 0;
        for (int i = 0; i < live; ++i) {
            if (live_al[static_cast<std::size_t>(i)] != ace_low(trips)) {
                rest[static_cast<std::size_t>(rn++)] = live_al[static_cast<std::size_t>(i)];
            }
        }
        write_rest(1, rest, rn);
        return e;
    }
    if (e.rank == HandRank::TwoPair) {
        e.kickers[0] = static_cast<std::uint8_t>(ace_low(pairs[1]));
        e.kickers[1] = static_cast<std::uint8_t>(ace_low(pairs[0]));
        std::array<int, 5> rest{};
        int rn = 0;
        for (int i = 0; i < live; ++i) {
            const int al = live_al[static_cast<std::size_t>(i)];
            if (al != ace_low(pairs[0]) && al != ace_low(pairs[1])) {
                rest[static_cast<std::size_t>(rn++)] = al;
            }
        }
        write_rest(2, rest, rn);
        return e;
    }
    if (e.rank == HandRank::OnePair) {
        e.kickers[0] = static_cast<std::uint8_t>(ace_low(pairs[0]));
        std::array<int, 5> rest{};
        int rn = 0;
        for (int i = 0; i < live; ++i) {
            if (live_al[static_cast<std::size_t>(i)] != ace_low(pairs[0])) {
                rest[static_cast<std::size_t>(rn++)] = live_al[static_cast<std::size_t>(i)];
            }
        }
        write_rest(1, rest, rn);
        return e;
    }
    std::array<int, 5> all{};
    for (int i = 0; i < live; ++i) {
        all[static_cast<std::size_t>(i)] = live_al[static_cast<std::size_t>(i)];
    }
    write_rest(0, all, live);
    return e;
}

RazzEvaluation evaluate_razz_n(const std::vector<Card>& cards) {
    const int n = static_cast<int>(cards.size());
    if (n <= 5) {
        Card buf[5]{};
        for (int i = 0; i < n; ++i) {
            buf[i] = cards[static_cast<std::size_t>(i)];
        }
        return evaluate_razz_five(buf, n);
    }
    RazzEvaluation best{};
    bool init = false;
    const int n_i = n;
    std::array<int, 5> idx{0, 1, 2, 3, 4};
    auto bump = [&]() -> bool {
        int i = 4;
        while (i >= 0 && idx[static_cast<std::size_t>(i)] == n_i - (5 - i)) {
            --i;
        }
        if (i < 0) {
            return false;
        }
        ++idx[static_cast<std::size_t>(i)];
        for (int j = i + 1; j < 5; ++j) {
            idx[static_cast<std::size_t>(j)] = idx[static_cast<std::size_t>(j - 1)] + 1;
        }
        return true;
    };
    do {
        Card five[5]{};
        for (int j = 0; j < 5; ++j) {
            five[j] = cards[static_cast<std::size_t>(idx[static_cast<std::size_t>(j)])];
        }
        const RazzEvaluation ev = evaluate_razz_five(five, 5);
        if (!init || ev < best) {
            best = ev;
            init = true;
        }
    } while (bump());
    return best;
}

void append_deck_ids(std::vector<Card>& hand, const int* ids, int n) {
    for (int i = 0; i < n; ++i) {
        hand.push_back(card_from_deck_index(ids[i]));
    }
}

std::uint64_t stud_strength(const std::vector<Card>& cards) {
    return pack_hand_strength(evaluate_best_hand(cards));
}

double showdown_share_high(std::uint64_t hero, std::uint64_t villain) {
    if (hero > villain) {
        return 1.0;
    }
    if (hero < villain) {
        return 0.0;
    }
    return 0.5;
}

double showdown_share_low(std::uint64_t hero, std::uint64_t villain) {
    if (hero < villain) {
        return 1.0;
    }
    if (hero > villain) {
        return 0.0;
    }
    return 0.5;
}

template <bool kRazz>
double exact_hu_complete(const std::vector<Card>& hero, const std::vector<Card>& villain,
                         const std::vector<Card>& extra_dead) {
    require_stud_len(hero, kRazz ? "exactHuRazzEquityVsKnown" : "exactHuStudEquityVsKnown");
    require_stud_len(villain, kRazz ? "exactHuRazzEquityVsKnown" : "exactHuStudEquityVsKnown");
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, villain);
    mark_unique(used, extra_dead);

    const int h_n = static_cast<int>(hero.size());
    const int v_n = static_cast<int>(villain.size());
    const int h_need = kStudMax - h_n;
    const int v_need = kStudMax - v_n;
    const std::vector<int> pool = used.unused_indices();
    const int d = static_cast<int>(pool.size());
    if (h_need + v_need > d) {
        throw std::invalid_argument("not enough cards to complete both hands to 7");
    }
    if (h_need == 0 && v_need == 0) {
        if constexpr (kRazz) {
            return showdown_share_low(pack_razz_strength(evaluate_razz_hand(hero)),
                                      pack_razz_strength(evaluate_razz_hand(villain)));
        } else {
            return showdown_share_high(stud_strength(hero), stud_strength(villain));
        }
    }

    const std::uint64_t deals = binom(d, h_need) * binom(d - h_need, v_need);
    if (deals == 0 || deals > kMaxExactDeals) {
        throw std::invalid_argument(
            "exact HU runout too large; use simulateStudEquityVsRandom / simulateRazzEquityVsRandom");
    }

    double win = 0.0;
    double total = 0.0;
    auto score = [&](std::vector<Card> h, std::vector<Card> v) {
        if constexpr (kRazz) {
            return showdown_share_low(pack_razz_strength(evaluate_razz_hand(h)),
                                      pack_razz_strength(evaluate_razz_hand(v)));
        } else {
            return showdown_share_high(stud_strength(h), stud_strength(v));
        }
    };

    if (h_need == 0) {
        for_each_combo_indices(pool, v_need, [&](const int* extra, int k) {
            std::vector<Card> v = villain;
            append_deck_ids(v, extra, k);
            win += score(hero, v);
            total += 1.0;
        });
    } else if (v_need == 0) {
        for_each_combo_indices(pool, h_need, [&](const int* extra, int k) {
            std::vector<Card> h = hero;
            append_deck_ids(h, extra, k);
            win += score(h, villain);
            total += 1.0;
        });
    } else {
        for_each_combo_indices(pool, h_need, [&](const int* h_extra, int hk) {
            // Villain completes from the live pool minus the cards hero just drew.
            std::vector<int> rest;
            rest.reserve(pool.size());
            for (int id : pool) {
                bool taken = false;
                for (int i = 0; i < hk; ++i) {
                    if (h_extra[i] == id) {
                        taken = true;
                        break;
                    }
                }
                if (!taken) {
                    rest.push_back(id);
                }
            }
            for_each_combo_indices(rest, v_need, [&](const int* v_extra, int vk) {
                std::vector<Card> h = hero;
                std::vector<Card> v = villain;
                append_deck_ids(h, h_extra, hk);
                append_deck_ids(v, v_extra, vk);
                win += score(std::move(h), std::move(v));
                total += 1.0;
            });
        });
    }
    if (total <= 0.0) {
        throw std::invalid_argument("empty HU enumeration");
    }
    return win / total;
}

template <bool kRazz>
float simulate_vs_random(const std::vector<Card>& hero, int trials, std::mt19937& rng,
                         const std::vector<Card>& extra_dead) {
    require_stud_len(hero, kRazz ? "simulateRazzEquityVsRandom" : "simulateStudEquityVsRandom");
    if (trials <= 0) {
        return 0.0F;
    }
    DeckBitset used;
    mark_unique(used, hero);
    mark_unique(used, extra_dead);
    const int h_n = static_cast<int>(hero.size());
    const int h_need = kStudMax - h_n;
    const int take = h_n + h_need + h_need;
    const std::vector<int> live0 = used.unused_indices();
    if (static_cast<int>(live0.size()) < take) {
        throw std::invalid_argument("not enough cards for random villain + runout");
    }
    double win = 0.0;
    for (int t = 0; t < trials; ++t) {
        std::vector<int> pool = live0;
        sample_without_replace(pool, take, rng);
        std::vector<Card> vil;
        vil.reserve(static_cast<std::size_t>(kStudMax));
        append_deck_ids(vil, pool.data(), h_n);
        std::vector<Card> h = hero;
        if (h_need > 0) {
            append_deck_ids(h, pool.data() + h_n, h_need);
            append_deck_ids(vil, pool.data() + h_n + h_need, h_need);
        }
        if constexpr (kRazz) {
            win += showdown_share_low(pack_razz_strength(evaluate_razz_hand(h)),
                                      pack_razz_strength(evaluate_razz_hand(vil)));
        } else {
            win += showdown_share_high(stud_strength(h), stud_strength(vil));
        }
    }
    return static_cast<float>(win / static_cast<double>(trials));
}

}  // namespace

bool RazzEvaluation::operator<(const RazzEvaluation& o) const {
    return pack_razz_strength(*this) < pack_razz_strength(o);
}

bool RazzEvaluation::operator==(const RazzEvaluation& o) const {
    return rank == o.rank && kickers == o.kickers;
}

std::uint64_t pack_razz_strength(const RazzEvaluation& e) {
    std::uint64_t x = static_cast<std::uint64_t>(pair_tier(e.rank)) << 24;
    for (int i = 0; i < 5; ++i) {
        x |= static_cast<std::uint64_t>(e.kickers[static_cast<std::size_t>(i)] & 0x1F) << (4 * (4 - i));
    }
    return x;
}

HandEvaluation evaluate_stud_best_hand(const std::vector<Card>& cards) {
    require_stud_len(cards, "evaluateStudBestHand");
    DeckBitset used;
    mark_unique(used, cards);
    return evaluate_best_hand(cards);
}

RazzEvaluation evaluate_razz_hand(const std::vector<Card>& cards) {
    require_stud_len(cards, "evaluateRazzHand");
    DeckBitset used;
    mark_unique(used, cards);
    return evaluate_razz_n(cards);
}

bool razz_wheel_is_nuts(const std::vector<Card>& cards) {
    require_stud_len(cards, "razzWheelIsNuts");
    DeckBitset used;
    mark_unique(used, cards);
    const RazzEvaluation e = evaluate_razz_n(cards);
    return e.rank == HandRank::HighCard && e.kickers[0] == 4 && e.kickers[1] == 3 && e.kickers[2] == 2 &&
           e.kickers[3] == 1 && e.kickers[4] == 0;
}

double exact_hu_stud_equity_vs_known(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                     const std::vector<Card>& extra_dead) {
    return exact_hu_complete<false>(hero, villain, extra_dead);
}

double exact_hu_razz_equity_vs_known(const std::vector<Card>& hero, const std::vector<Card>& villain,
                                     const std::vector<Card>& extra_dead) {
    return exact_hu_complete<true>(hero, villain, extra_dead);
}

std::vector<std::string> stud_dead_card_deck(const std::vector<Card>& dead) {
    DeckBitset used;
    mark_unique(used, dead);
    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(52 - used.count()));
    for (int id : used.unused_indices()) {
        out.push_back(card_from_deck_index(id).to_string());
    }
    return out;
}

float simulate_stud_equity_vs_random(const std::vector<Card>& hero, int trials, std::mt19937& rng,
                                     const std::vector<Card>& extra_dead) {
    return simulate_vs_random<false>(hero, trials, rng, extra_dead);
}

float simulate_razz_equity_vs_random(const std::vector<Card>& hero, int trials, std::mt19937& rng,
                                     const std::vector<Card>& extra_dead) {
    return simulate_vs_random<true>(hero, trials, rng, extra_dead);
}

}  // namespace poker
