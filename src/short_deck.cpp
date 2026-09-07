#include "poker/short_deck.hpp"

#include "poker/bit_utils.hpp"
#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/hand_evaluator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace poker {
namespace {

constexpr int kLiveLo = 16;  // 6c
constexpr int kLiveHi = 52;

int straight_high_short(const std::array<int, 13>& present) {
    for (int high = 12; high >= 8; --high) {
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
    // Wheel A-6-7-8-9 (nine-high). No A2345.
    if (present[12] && present[4] && present[5] && present[6] && present[7]) {
        return 7;
    }
    return -1;
}

bool is_flush_five(const std::vector<Card>& five) {
    const std::uint8_t s0 = five[0].suit();
    for (const Card& c : five) {
        if (c.suit() != s0) {
            return false;
        }
    }
    return true;
}

ShortDeckEvaluation evaluate_sorted_five(const std::vector<Card>& v) {
    ShortDeckEvaluation e{};
    std::array<int, 13> freq{};
    for (const Card& c : v) {
        freq[static_cast<std::size_t>(c.rank())]++;
    }
    std::array<int, 13> present{};
    for (int r = 0; r < 13; ++r) {
        present[static_cast<std::size_t>(r)] = freq[static_cast<std::size_t>(r)] > 0 ? 1 : 0;
    }

    std::vector<std::pair<int, int>> groups;
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

    const bool flush = is_flush_five(v);
    const int sh = straight_high_short(present);

    if (flush && sh >= 0) {
        e.rank = (sh == 12) ? ShortDeckRank::RoyalFlush : ShortDeckRank::StraightFlush;
        e.kickers[0] = static_cast<std::uint8_t>(sh);
        return e;
    }

    if (!groups.empty() && groups[0].second == 4) {
        e.rank = ShortDeckRank::FourOfAKind;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (std::size_t i = 1; i < groups.size() && ki < 5; ++i) {
            for (int t = 0; t < groups[i].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(groups[i].first);
            }
        }
        return e;
    }

    // Flush beats boat: assign flush after quads, before boat, so a 5-card hand that
    // is only one of the two gets the right label; 7-card best-of-five uses operator<
    // (flush ordinal > boat ordinal).
    if (flush) {
        e.rank = ShortDeckRank::Flush;
        int ki = 0;
        for (const auto& g : groups) {
            for (int t = 0; t < g.second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(g.first);
            }
        }
        return e;
    }

    if (groups.size() >= 2 && groups[0].second == 3 && groups[1].second == 2) {
        e.rank = ShortDeckRank::FullHouse;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        e.kickers[1] = static_cast<std::uint8_t>(groups[1].first);
        return e;
    }

    if (sh >= 0) {
        e.rank = ShortDeckRank::Straight;
        e.kickers[0] = static_cast<std::uint8_t>(sh);
        return e;
    }

    if (!groups.empty() && groups[0].second == 3) {
        e.rank = ShortDeckRank::ThreeOfAKind;
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
        e.rank = ShortDeckRank::TwoPair;
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
        e.rank = ShortDeckRank::OnePair;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (std::size_t i = 1; i < groups.size() && ki < 5; ++i) {
            for (int t = 0; t < groups[i].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(groups[i].first);
            }
        }
        return e;
    }

    e.rank = ShortDeckRank::HighCard;
    int ki = 0;
    for (const auto& g : groups) {
        for (int t = 0; t < g.second && ki < 5; ++t) {
            e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(g.first);
        }
    }
    return e;
}

ShortDeckEvaluation evaluate_five_array(std::array<Card, 5> five) {
    std::sort(five.begin(), five.end(), [](const Card& a, const Card& b) {
        if (a.rank() != b.rank()) {
            return a.rank() > b.rank();
        }
        return a.suit() < b.suit();
    });
    return evaluate_sorted_five(std::vector<Card>(five.begin(), five.end()));
}

ShortDeckEvaluation partial_high(const std::vector<Card>& cards) {
    ShortDeckEvaluation e{};
    e.rank = ShortDeckRank::HighCard;
    std::vector<Card> v = cards;
    std::sort(v.begin(), v.end(), [](const Card& a, const Card& b) {
        if (a.rank() != b.rank()) {
            return a.rank() > b.rank();
        }
        return a.suit() < b.suit();
    });
    int ki = 0;
    for (const Card& c : v) {
        if (ki >= 5) {
            break;
        }
        e.kickers[static_cast<std::size_t>(ki++)] = c.rank();
    }
    return e;
}

std::vector<int> live_indices(std::uint64_t dead) {
    std::vector<int> out;
    out.reserve(kShortDeckCardCount);
    for (int i = kLiveLo; i < kLiveHi; ++i) {
        if (((dead >> i) & 1ULL) == 0) {
            out.push_back(i);
        }
    }
    return out;
}

std::uint64_t mark_dead(const std::vector<Card>& cards) {
    std::uint64_t m = 0;
    for (const Card& c : cards) {
        m |= (1ULL << deck_index_from_card(c));
    }
    return m;
}

int cmp_eval(const ShortDeckEvaluation& a, const ShortDeckEvaluation& b) {
    if (a < b) {
        return -1;
    }
    if (b < a) {
        return 1;
    }
    return 0;
}

ShortDeckEvaluation eval_seven(int h0, int h1, const std::vector<Card>& board, const int* run, int run_k) {
    std::vector<Card> all;
    all.reserve(7);
    all.push_back(card_from_deck_index(h0));
    all.push_back(card_from_deck_index(h1));
    all.insert(all.end(), board.begin(), board.end());
    for (int i = 0; i < run_k; ++i) {
        all.push_back(card_from_deck_index(run[i]));
    }
    return evaluate_short_deck_best_hand(all);
}

void decode_hand81(int hand81, int& high_rank, int& low_rank, bool& suited) {
    if (hand81 < 0 || hand81 >= kShortDeckHandClasses) {
        throw std::invalid_argument("short-deck hand class out of range");
    }
    int idx = 0;
    for (int i = kShortDeckMinRank; i <= 12; ++i) {
        for (int j = i; j <= 12; ++j) {
            if (i == j) {
                if (idx == hand81) {
                    high_rank = i;
                    low_rank = j;
                    suited = false;
                    return;
                }
                ++idx;
            } else {
                if (idx == hand81) {
                    high_rank = j;
                    low_rank = i;
                    suited = true;
                    return;
                }
                ++idx;
                if (idx == hand81) {
                    high_rank = j;
                    low_rank = i;
                    suited = false;
                    return;
                }
                ++idx;
            }
        }
    }
    throw std::invalid_argument("short-deck hand class decode failed");
}

bool pick_holes(int high, int low, bool suited, std::uint64_t dead, int& c0, int& c1) {
    for (int s_high = 0; s_high < 4; ++s_high) {
        for (int s_low = 0; s_low < 4; ++s_low) {
            if (high == low && s_low == s_high) {
                continue;
            }
            if (high != low && suited && s_low != s_high) {
                continue;
            }
            if (high != low && !suited && s_low == s_high) {
                continue;
            }
            const int i0 = high * 4 + s_high;
            const int i1 = low * 4 + (suited ? s_high : s_low);
            if (i0 == i1) {
                continue;
            }
            const std::uint64_t bit = (1ULL << i0) | (1ULL << i1);
            if ((dead & bit) != 0) {
                continue;
            }
            c0 = i0;
            c1 = i1;
            return true;
        }
    }
    return false;
}

double combo_weight_from_ranks(int high, int low, bool suited) {
    if (high == low) {
        return 6.0;
    }
    return suited ? 4.0 : 12.0;
}

double matchup_equity(int h0, int h1, int v0, int v1, int iterations, std::mt19937& rng) {
    std::array<int, 32> live{};
    int n = 0;
    const std::uint64_t dead = (1ULL << h0) | (1ULL << h1) | (1ULL << v0) | (1ULL << v1);
    for (int i = kLiveLo; i < kLiveHi; ++i) {
        if (((dead >> i) & 1ULL) == 0) {
            live[static_cast<std::size_t>(n++)] = i;
        }
    }
    double sum = 0.0;
    for (int t = 0; t < iterations; ++t) {
        for (int k = 0; k < 5; ++k) {
            const int j = k + static_cast<int>(rng() % static_cast<unsigned>(n - k));
            std::swap(live[static_cast<std::size_t>(k)], live[static_cast<std::size_t>(j)]);
        }
        const ShortDeckEvaluation eh = eval_seven(h0, h1, {}, live.data(), 5);
        const ShortDeckEvaluation ev = eval_seven(v0, v1, {}, live.data(), 5);
        const int cmp = cmp_eval(eh, ev);
        if (cmp > 0) {
            sum += 1.0;
        } else if (cmp == 0) {
            sum += 0.5;
        }
    }
    return sum / static_cast<double>(iterations);
}

void fill_equity_matrix(int iterations, std::uint32_t seed, std::vector<double>& out) {
    constexpr int kHands = kShortDeckHandClasses;
    out.assign(static_cast<std::size_t>(kHands * kHands), 0.5);
    for (int i = 0; i < kHands; ++i) {
        int hi = 0;
        int lo = 0;
        bool suited = false;
        decode_hand81(i, hi, lo, suited);
        int h0 = 0;
        int h1 = 0;
        if (!pick_holes(hi, lo, suited, 0, h0, h1)) {
            continue;
        }
        const std::uint64_t hero_dead = (1ULL << h0) | (1ULL << h1);
        for (int j = i + 1; j < kHands; ++j) {
            int vhi = 0;
            int vlo = 0;
            bool vs = false;
            decode_hand81(j, vhi, vlo, vs);
            int v0 = 0;
            int v1 = 0;
            if (!pick_holes(vhi, vlo, vs, hero_dead, v0, v1)) {
                continue;
            }
            std::mt19937 rng(seed + static_cast<std::uint32_t>(i * 617 + j * 991));
            const double e = matchup_equity(h0, h1, v0, v1, iterations, rng);
            out[static_cast<std::size_t>(i * kHands + j)] = e;
            out[static_cast<std::size_t>(j * kHands + i)] = 1.0 - e;
        }
    }
}

const std::vector<double>& cached_equity_matrix(int iterations, std::uint32_t seed) {
    static std::mutex mu;
    static int cached_iters = -1;
    static std::uint32_t cached_seed = 0;
    static std::vector<double> cached;
    if (iterations < 1) {
        throw std::invalid_argument("equityIterations must be positive");
    }
    std::lock_guard<std::mutex> lock(mu);
    if (cached_iters == iterations && cached_seed == seed &&
        cached.size() == static_cast<std::size_t>(kShortDeckHandClasses * kShortDeckHandClasses)) {
        return cached;
    }
    fill_equity_matrix(iterations, seed, cached);
    cached_iters = iterations;
    cached_seed = seed;
    return cached;
}

const std::array<double, kShortDeckHandClasses>& combo_weights() {
    static const std::array<double, kShortDeckHandClasses> w = [] {
        std::array<double, kShortDeckHandClasses> out{};
        for (int h = 0; h < kShortDeckHandClasses; ++h) {
            int hi = 0;
            int lo = 0;
            bool suited = false;
            decode_hand81(h, hi, lo, suited);
            out[static_cast<std::size_t>(h)] = combo_weight_from_ranks(hi, lo, suited);
        }
        return out;
    }();
    return w;
}

double weight_sum(const std::array<double, kShortDeckHandClasses>& freq) {
    const auto& w = combo_weights();
    double s = 0.0;
    for (int i = 0; i < kShortDeckHandClasses; ++i) {
        s += w[static_cast<std::size_t>(i)] * freq[static_cast<std::size_t>(i)];
    }
    return s;
}

double equity_vs_freq(int hand, const std::array<double, kShortDeckHandClasses>& freq,
                      const std::vector<double>& matrix) {
    const auto& w = combo_weights();
    double num = 0.0;
    double den = 0.0;
    const std::size_t row = static_cast<std::size_t>(hand) * static_cast<std::size_t>(kShortDeckHandClasses);
    for (int j = 0; j < kShortDeckHandClasses; ++j) {
        const double wt = w[static_cast<std::size_t>(j)] * freq[static_cast<std::size_t>(j)];
        if (wt <= 0.0) {
            continue;
        }
        num += wt * matrix[row + static_cast<std::size_t>(j)];
        den += wt;
    }
    return den > 0.0 ? num / den : 0.5;
}

double br_from_ev(double ev, double fold_ev, double tol) {
    const double d = ev - fold_ev;
    if (d > tol) {
        return 1.0;
    }
    if (d < -tol) {
        return 0.0;
    }
    return 0.5;
}

void mix_avg(std::array<double, kShortDeckHandClasses>& avg,
             const std::array<double, kShortDeckHandClasses>& br, int iter) {
    const double n = static_cast<double>(iter);
    for (int i = 0; i < kShortDeckHandClasses; ++i) {
        avg[static_cast<std::size_t>(i)] =
            (avg[static_cast<std::size_t>(i)] * n + br[static_cast<std::size_t>(i)]) / (n + 1.0);
    }
}

int parse_rank_char(const std::string& s, std::size_t& i) {
    if (i >= s.size()) {
        return -1;
    }
    const char c0 = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    if (c0 == '1' && i + 1 < s.size() && s[i + 1] == '0') {
        i += 2;
        return 8;
    }
    static constexpr const char* kRanks = "23456789TJQKA";
    for (int k = 0; k < 13; ++k) {
        if (kRanks[k] == c0) {
            ++i;
            return k;
        }
    }
    return -1;
}

std::uint64_t pack_strength(const ShortDeckEvaluation& e) {
    std::uint64_t x = static_cast<std::uint64_t>(e.rank) << 24;
    for (int i = 0; i < 5; ++i) {
        x |= static_cast<std::uint64_t>(e.kickers[static_cast<std::size_t>(i)] & 0x1F) << (4 * (4 - i));
    }
    return x;
}

double showdown_share(const ShortDeckEvaluation& hero, const ShortDeckEvaluation& villain) {
    const int cmp = cmp_eval(hero, villain);
    if (cmp > 0) {
        return 1.0;
    }
    if (cmp == 0) {
        return 0.5;
    }
    return 0.0;
}

void sample_board(std::vector<int>& live, int need, std::mt19937& rng, int* out) {
    for (int k = 0; k < need; ++k) {
        const int j = k + static_cast<int>(rng() % static_cast<unsigned>(static_cast<int>(live.size()) - k));
        std::swap(live[static_cast<std::size_t>(k)], live[static_cast<std::size_t>(j)]);
        out[k] = live[static_cast<std::size_t>(k)];
    }
}

}  // namespace

bool ShortDeckEvaluation::operator<(const ShortDeckEvaluation& o) const {
    if (rank != o.rank) {
        return static_cast<int>(rank) < static_cast<int>(o.rank);
    }
    return kickers < o.kickers;
}

bool ShortDeckEvaluation::operator==(const ShortDeckEvaluation& o) const {
    return rank == o.rank && kickers == o.kickers;
}

bool is_short_deck_card(const Card& c) {
    return c.rank() >= kShortDeckMinRank;
}

void require_short_deck_cards(const std::vector<Card>& cards, const char* ctx, int min_n, int max_n) {
    const int n = static_cast<int>(cards.size());
    if (n < min_n || n > max_n) {
        throw std::invalid_argument(std::string(ctx) + ": expected " + std::to_string(min_n) + ".." +
                                    std::to_string(max_n) + " cards");
    }
    bool seen[52]{};
    for (const Card& c : cards) {
        if (!is_short_deck_card(c)) {
            throw std::invalid_argument("short deck (6+) rejects ranks 2-5");
        }
        const int idx = deck_index_from_card(c);
        if (idx < 0 || idx > 51 || seen[idx]) {
            throw std::invalid_argument(std::string(ctx) + ": duplicate or invalid card");
        }
        seen[idx] = true;
    }
}

ShortDeckEvaluation evaluate_short_deck_five(std::vector<Card> five) {
    require_short_deck_cards(five, "evaluateShortDeck", 5, 5);
    std::sort(five.begin(), five.end(), [](const Card& a, const Card& b) {
        if (a.rank() != b.rank()) {
            return a.rank() > b.rank();
        }
        return a.suit() < b.suit();
    });
    return evaluate_sorted_five(five);
}

ShortDeckEvaluation evaluate_short_deck_best_hand(const std::vector<Card>& cards) {
    require_short_deck_cards(cards, "evaluateShortDeckBestHand", 1, 7);
    if (cards.size() < 5) {
        return partial_high(cards);
    }
    ShortDeckEvaluation best{};
    bool init = false;
    const int n = static_cast<int>(cards.size());
    std::array<int, 5> idx{0, 1, 2, 3, 4};
    auto bump = [&]() -> bool {
        int i = 4;
        while (i >= 0 && idx[static_cast<std::size_t>(i)] == n - (5 - i)) {
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
        std::array<Card, 5> five{};
        for (int j = 0; j < 5; ++j) {
            five[static_cast<std::size_t>(j)] = cards[static_cast<std::size_t>(idx[static_cast<std::size_t>(j)])];
        }
        const ShortDeckEvaluation ev = evaluate_five_array(five);
        if (!init || best < ev) {
            best = ev;
            init = true;
        }
    } while (bump());
    return best;
}

std::uint64_t evaluate_short_deck_hand_strength(const std::vector<Card>& player_hand,
                                                const std::vector<Card>& community_cards) {
    std::vector<Card> all = player_hand;
    all.insert(all.end(), community_cards.begin(), community_cards.end());
    require_short_deck_cards(all, "evaluateShortDeckHandStrength", 1, 7);
    return pack_strength(evaluate_short_deck_best_hand(all));
}

ShortDeckRank evaluate_short_deck_category(const std::vector<Card>& player_hand,
                                           const std::vector<Card>& community_cards) {
    std::vector<Card> all = player_hand;
    all.insert(all.end(), community_cards.begin(), community_cards.end());
    require_short_deck_cards(all, "evaluateShortDeckCategory", 1, 7);
    return evaluate_short_deck_best_hand(all).rank;
}

const char* short_deck_rank_label(ShortDeckRank r) {
    switch (r) {
        case ShortDeckRank::HighCard:
            return "highCard";
        case ShortDeckRank::OnePair:
            return "onePair";
        case ShortDeckRank::TwoPair:
            return "twoPair";
        case ShortDeckRank::ThreeOfAKind:
            return "threeOfAKind";
        case ShortDeckRank::Straight:
            return "straight";
        case ShortDeckRank::FullHouse:
            return "fullHouse";
        case ShortDeckRank::Flush:
            return "flush";
        case ShortDeckRank::FourOfAKind:
            return "fourOfAKind";
        case ShortDeckRank::StraightFlush:
            return "straightFlush";
        case ShortDeckRank::RoyalFlush:
            return "royalFlush";
        default:
            return "unknown";
    }
}

HandRank short_deck_rank_to_label(ShortDeckRank r) {
    switch (r) {
        case ShortDeckRank::FullHouse:
            return HandRank::FullHouse;
        case ShortDeckRank::Flush:
            return HandRank::Flush;
        default:
            return static_cast<HandRank>(static_cast<int>(r));
    }
}

bool short_deck_straight_is_wheel(const std::vector<Card>& five) {
    require_short_deck_cards(five, "shortDeckStraightIsWheel", 5, 5);
    const ShortDeckEvaluation e = evaluate_short_deck_five(five);
    if (e.rank != ShortDeckRank::Straight && e.rank != ShortDeckRank::StraightFlush) {
        return false;
    }
    return e.kickers[0] == 7;
}

int short_deck_remaining_combo_count(const std::vector<Card>& dead) {
    require_short_deck_cards(dead, "shortDeckRemainingComboCount", 0, kShortDeckCardCount);
    const int n = kShortDeckCardCount - static_cast<int>(dead.size());
    if (n < 2) {
        return 0;
    }
    return n * (n - 1) / 2;
}

double exact_hu_short_deck_equity_vs_known(const std::vector<Card>& hero_hole,
                                           const std::vector<Card>& villain_hole,
                                           const std::vector<Card>& board) {
    require_short_deck_cards(hero_hole, "exactHuShortDeckEquityVsKnown hero", 2, 2);
    require_short_deck_cards(villain_hole, "exactHuShortDeckEquityVsKnown villain", 2, 2);
    require_short_deck_cards(board, "exactHuShortDeckEquityVsKnown board", 0, 5);
    const std::uint64_t dead = mark_dead(hero_hole) | mark_dead(villain_hole) | mark_dead(board);
    const int uniq = static_cast<int>(hero_hole.size() + villain_hole.size() + board.size());
    if (popcount_u64(dead) != uniq) {
        throw std::invalid_argument("exactHuShortDeckEquityVsKnown: duplicate cards");
    }
    const int h0 = deck_index_from_card(hero_hole[0]);
    const int h1 = deck_index_from_card(hero_hole[1]);
    const int v0 = deck_index_from_card(villain_hole[0]);
    const int v1 = deck_index_from_card(villain_hole[1]);
    const int need = 5 - static_cast<int>(board.size());
    const std::vector<int> live = live_indices(dead);
    if (need == 0) {
        return showdown_share(eval_seven(h0, h1, board, nullptr, 0), eval_seven(v0, v1, board, nullptr, 0));
    }
    if (static_cast<int>(live.size()) < need) {
        throw std::invalid_argument("exactHuShortDeckEquityVsKnown: not enough cards to complete board");
    }
    double win = 0.0;
    double total = 0.0;
    for_each_combo_indices(live, need, [&](const int* run, int run_k) {
        const double share =
            showdown_share(eval_seven(h0, h1, board, run, run_k), eval_seven(v0, v1, board, run, run_k));
        win += share;
        total += 1.0;
    });
    return total > 0.0 ? win / total : 0.0;
}

double simulate_short_deck_equity_vs_random(const std::vector<Card>& hero_hole, const std::vector<Card>& board,
                                            int num_simulations, std::mt19937& rng) {
    require_short_deck_cards(hero_hole, "simulateShortDeckEquityVsRandom hero", 2, 2);
    require_short_deck_cards(board, "simulateShortDeckEquityVsRandom board", 0, 5);
    if (num_simulations <= 0) {
        throw std::invalid_argument("numSimulations must be positive");
    }
    const std::uint64_t dead = mark_dead(hero_hole) | mark_dead(board);
    if (popcount_u64(dead) != static_cast<int>(hero_hole.size() + board.size())) {
        throw std::invalid_argument("simulateShortDeckEquityVsRandom: duplicate cards");
    }
    const int h0 = deck_index_from_card(hero_hole[0]);
    const int h1 = deck_index_from_card(hero_hole[1]);
    const int need_board = 5 - static_cast<int>(board.size());
    const std::vector<int> base = live_indices(dead);
    if (static_cast<int>(base.size()) < 2 + need_board) {
        throw std::invalid_argument("simulateShortDeckEquityVsRandom: not enough cards");
    }
    double sum = 0.0;
    for (int t = 0; t < num_simulations; ++t) {
        std::vector<int> live = base;
        for (int k = 0; k < 2; ++k) {
            const int j = k + static_cast<int>(rng() % static_cast<unsigned>(static_cast<int>(live.size()) - k));
            std::swap(live[static_cast<std::size_t>(k)], live[static_cast<std::size_t>(j)]);
        }
        const int v0 = live[0];
        const int v1 = live[1];
        int run[5]{};
        if (need_board > 0) {
            std::vector<int> rest(live.begin() + 2, live.end());
            sample_board(rest, need_board, rng, run);
        }
        sum += showdown_share(eval_seven(h0, h1, board, run, need_board),
                              eval_seven(v0, v1, board, run, need_board));
    }
    return sum / static_cast<double>(num_simulations);
}

double simulate_short_deck_equity_vs_range(const std::vector<Card>& hero_hole, const std::vector<Card>& board,
                                           const SparseRange& villain_range, int num_simulations, std::mt19937& rng) {
    require_short_deck_cards(hero_hole, "simulateShortDeckEquityVsRange hero", 2, 2);
    require_short_deck_cards(board, "simulateShortDeckEquityVsRange board", 0, 5);
    if (num_simulations <= 0) {
        throw std::invalid_argument("numSimulations must be positive");
    }
    if (villain_range.combos.empty() || villain_range.weight_sum <= 0.0) {
        throw std::invalid_argument("simulateShortDeckEquityVsRange: empty range");
    }
    std::uint64_t dead = mark_dead(hero_hole) | mark_dead(board);
    const int h0 = deck_index_from_card(hero_hole[0]);
    const int h1 = deck_index_from_card(hero_hole[1]);
    const int need_board = 5 - static_cast<int>(board.size());
    double sum = 0.0;
    int used = 0;
    for (int t = 0; t < num_simulations; ++t) {
        std::uniform_real_distribution<double> dist(0.0, villain_range.weight_sum);
        const double pick = dist(rng);
        double acc = 0.0;
        const WeightedHoleCombo* chosen = &villain_range.combos.back();
        for (const WeightedHoleCombo& c : villain_range.combos) {
            acc += c.weight;
            if (pick <= acc) {
                chosen = &c;
                break;
            }
        }
        if (((dead >> chosen->card_a) & 1ULL) != 0 || ((dead >> chosen->card_b) & 1ULL) != 0) {
            continue;
        }
        if (chosen->card_a < kLiveLo || chosen->card_b < kLiveLo) {
            continue;
        }
        std::uint64_t used_mask = dead | (1ULL << chosen->card_a) | (1ULL << chosen->card_b);
        std::vector<int> live = live_indices(used_mask);
        if (static_cast<int>(live.size()) < need_board) {
            continue;
        }
        int run[5]{};
        if (need_board > 0) {
            sample_board(live, need_board, rng, run);
        }
        sum += showdown_share(eval_seven(h0, h1, board, run, need_board),
                              eval_seven(chosen->card_a, chosen->card_b, board, run, need_board));
        ++used;
    }
    if (used <= 0) {
        throw std::invalid_argument("simulateShortDeckEquityVsRange: no unblocked combos");
    }
    return sum / static_cast<double>(used);
}

SparseRange short_deck_range_from_class_weights(const std::array<double, kShortDeckHandClasses>& weights,
                                                std::uint64_t dead_mask) {
    SparseRange out;
    for (int h = 0; h < kShortDeckHandClasses; ++h) {
        const double w = weights[static_cast<std::size_t>(h)];
        if (w <= 0.0) {
            continue;
        }
        int hi = 0;
        int lo = 0;
        bool suited = false;
        decode_hand81(h, hi, lo, suited);
        for (int s_high = 0; s_high < 4; ++s_high) {
            for (int s_low = 0; s_low < 4; ++s_low) {
                if (hi == lo && s_low <= s_high) {
                    continue;
                }
                if (hi != lo && suited && s_low != s_high) {
                    continue;
                }
                if (hi != lo && !suited && s_low == s_high) {
                    continue;
                }
                int i0 = hi * 4 + s_high;
                int i1 = lo * 4 + (hi == lo ? s_low : (suited ? s_high : s_low));
                if (i0 == i1) {
                    continue;
                }
                if (i0 > i1) {
                    std::swap(i0, i1);
                }
                if (((dead_mask >> i0) & 1ULL) != 0 || ((dead_mask >> i1) & 1ULL) != 0) {
                    continue;
                }
                out.combos.push_back(WeightedHoleCombo{i0, i1, w});
                out.weight_sum += w;
            }
        }
    }
    return out;
}

int short_deck_hand81_index(int high_rank, int low_rank, bool suited) {
    if (high_rank < kShortDeckMinRank || high_rank > 12 || low_rank < kShortDeckMinRank || low_rank > 12) {
        throw std::invalid_argument("short-deck class requires ranks 6-A");
    }
    int high = high_rank;
    int low = low_rank;
    if (high < low) {
        std::swap(high, low);
    }
    if (high == low && suited) {
        throw std::invalid_argument("pairs have no suited/offsuit suffix");
    }
    int idx = 0;
    for (int i = kShortDeckMinRank; i <= 12; ++i) {
        for (int j = i; j <= 12; ++j) {
            if (i == j) {
                if (high == i && low == j) {
                    return idx;
                }
                ++idx;
            } else {
                if (high == j && low == i && suited) {
                    return idx;
                }
                ++idx;
                if (high == j && low == i && !suited) {
                    return idx;
                }
                ++idx;
            }
        }
    }
    throw std::invalid_argument("short-deck hand81 index not found");
}

int short_deck_hand81_from_notation(const std::string& notation_raw) {
    std::string s;
    s.reserve(notation_raw.size());
    for (unsigned char ch : notation_raw) {
        if (!std::isspace(ch)) {
            s.push_back(static_cast<char>(ch));
        }
    }
    if (s.empty()) {
        throw std::invalid_argument("empty hand notation");
    }
    std::size_t i = 0;
    const int r1 = parse_rank_char(s, i);
    const int r2 = parse_rank_char(s, i);
    if (r1 < 0 || r2 < 0) {
        throw std::invalid_argument("invalid hand notation rank");
    }
    if (r1 < kShortDeckMinRank || r2 < kShortDeckMinRank) {
        throw std::invalid_argument("short-deck class requires ranks 6-A");
    }
    if (i == s.size()) {
        if (r1 != r2) {
            throw std::invalid_argument("non-pair notation needs s/o suffix");
        }
        return short_deck_hand81_index(r1, r2, false);
    }
    if (i + 1 != s.size()) {
        throw std::invalid_argument("invalid hand notation");
    }
    const char suf = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    if (r1 == r2) {
        throw std::invalid_argument("pair notation must omit s/o");
    }
    if (suf == 's') {
        return short_deck_hand81_index(r1, r2, true);
    }
    if (suf == 'o') {
        return short_deck_hand81_index(r1, r2, false);
    }
    throw std::invalid_argument("hand notation suffix must be s or o");
}

int short_deck_hand81_from_hand169(int hand169) {
    if (hand169 < 0 || hand169 > 168) {
        return -1;
    }
    int idx = 0;
    for (int i = 0; i < 13; ++i) {
        for (int j = i; j < 13; ++j) {
            if (i == j) {
                if (idx == hand169) {
                    if (i < kShortDeckMinRank) {
                        return -1;
                    }
                    return short_deck_hand81_index(i, j, false);
                }
                ++idx;
            } else {
                if (idx == hand169) {
                    if (i < kShortDeckMinRank || j < kShortDeckMinRank) {
                        return -1;
                    }
                    return short_deck_hand81_index(j, i, true);
                }
                ++idx;
                if (idx == hand169) {
                    if (i < kShortDeckMinRank || j < kShortDeckMinRank) {
                        return -1;
                    }
                    return short_deck_hand81_index(j, i, false);
                }
                ++idx;
            }
        }
    }
    return -1;
}

std::array<double, kShortDeckHandClasses> short_deck_nash_hu_jam_range(const ShortDeckNashSpec& spec) {
    if (!(spec.big_blind > 0.0) || !std::isfinite(spec.big_blind)) {
        throw std::invalid_argument("bigBlind must be positive");
    }
    if (!(spec.hero_stack > spec.hero_posted) || !(spec.villain_stack > spec.villain_posted)) {
        throw std::invalid_argument("stack must exceed posted blind");
    }
    const int iters = std::max(1, std::min(spec.max_iterations, kShortDeckNashMaxIterations));
    const auto& matrix = cached_equity_matrix(spec.equity_iterations, spec.equity_seed);
    const double pot = spec.hero_posted + spec.villain_posted + spec.ante;
    const double eff = std::min(spec.hero_stack - spec.hero_posted, spec.villain_stack - spec.villain_posted);
    if (!(pot > 0.0) || !(eff > 0.0)) {
        throw std::invalid_argument("pot and effective remaining stack must be positive");
    }
    constexpr int kHands = kShortDeckHandClasses;
    std::array<double, kHands> ones{};
    ones.fill(1.0);
    const double w_all = weight_sum(ones);
    std::array<double, kHands> jam_avg{};
    std::array<double, kHands> call_avg{};
    jam_avg.fill(0.5);
    call_avg.fill(0.5);
    std::array<double, kHands> jam_br{};
    std::array<double, kHands> call_br{};
    auto chip_called = [&](double eq) { return eq * pot + (2.0 * eq - 1.0) * eff; };
    auto chip_jam = [&](double eq, double p_call) {
        return (1.0 - p_call) * pot + p_call * chip_called(eq);
    };
    for (int iter = 0; iter < iters; ++iter) {
        const double p_call = weight_sum(call_avg) / w_all;
        for (int i = 0; i < kHands; ++i) {
            const double eq = equity_vs_freq(i, call_avg, matrix);
            jam_br[static_cast<std::size_t>(i)] = br_from_ev(chip_jam(eq, p_call), 0.0, spec.tolerance);
        }
        for (int j = 0; j < kHands; ++j) {
            const double eq = equity_vs_freq(j, jam_avg, matrix);
            call_br[static_cast<std::size_t>(j)] = br_from_ev(chip_called(eq), 0.0, spec.tolerance);
        }
        mix_avg(jam_avg, jam_br, iter);
        mix_avg(call_avg, call_br, iter);
    }
    return jam_avg;
}

bool short_deck_vs_holdem_category_flip(const std::vector<Card>& cards) {
    require_short_deck_cards(cards, "shortDeckVsHoldemCategoryFlip", 5, 7);
    const HandRank holdem = evaluate_best_hand(cards).rank;
    const ShortDeckRank six_plus = evaluate_short_deck_best_hand(cards).rank;
    return short_deck_rank_to_label(six_plus) != holdem;
}

}  // namespace poker
