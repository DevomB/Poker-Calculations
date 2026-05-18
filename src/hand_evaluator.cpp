#include "poker/hand_evaluator.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace poker {

namespace {

std::vector<Card> sort_cards_desc(const std::vector<Card>& in) {
    std::vector<Card> v = in;
    std::sort(v.begin(), v.end(), [](const Card& a, const Card& b) {
        if (a.rank() != b.rank()) {
            return a.rank() > b.rank();
        }
        return a.suit() < b.suit();
    });
    return v;
}

bool is_flush_five(const std::vector<Card>& five) {
    std::uint8_t s0 = five[0].suit();
    for (const auto& c : five) {
        if (c.suit() != s0) {
            return false;
        }
    }
    return true;
}

int straight_high_from_mask(const std::array<int, 13>& present) {
    if (present[12] && present[0] && present[1] && present[2] && present[3]) {
        return 3;
    }
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

HandEvaluation evaluate_sorted_five(std::vector<Card> v) {
    HandEvaluation e{};
    std::array<int, 13> freq{};
    for (const auto& c : v) {
        freq[static_cast<std::size_t>(c.rank())]++;
    }
    std::array<int, 13> present{};
    for (int r = 0; r < 13; ++r) {
        present[static_cast<std::size_t>(r)] = freq[static_cast<std::size_t>(r)] > 0 ? 1 : 0;
    }

    std::vector<std::pair<int, int>> groups;
    for (int r = 12; r >= 0; --r) {
        int c = freq[static_cast<std::size_t>(r)];
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
    const int sh = straight_high_from_mask(present);

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
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[i].first);
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
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(g.first);
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
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[i].first);
            }
        }
        return e;
    }

    std::vector<int> pair_ranks;
    pair_ranks.reserve(2);
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
            if (g.second != 1) {
                continue;
            }
            e.kickers[static_cast<std::size_t>(ki++)] = static_cast<std::uint8_t>(g.first);
        }
        return e;
    }

    if (!groups.empty() && groups[0].second == 2) {
        e.rank = HandRank::OnePair;
        e.kickers[0] = static_cast<std::uint8_t>(groups[0].first);
        int ki = 1;
        for (std::size_t i = 1; i < groups.size() && ki < 5; ++i) {
            for (int t = 0; t < groups[i].second && ki < 5; ++t) {
                e.kickers[static_cast<std::size_t>(ki++)] =
                    static_cast<std::uint8_t>(groups[i].first);
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

HandEvaluation partial_high_card(const std::vector<Card>& cards) {
    HandEvaluation e{};
    e.rank = HandRank::HighCard;
    auto v = sort_cards_desc(cards);
    int ki = 0;
    for (const auto& c : v) {
        if (ki >= 5) {
            break;
        }
        e.kickers[static_cast<std::size_t>(ki++)] = c.rank();
    }
    return e;
}

}  // namespace

bool HandEvaluation::operator<(const HandEvaluation& o) const {
    if (rank != o.rank) {
        return static_cast<int>(rank) < static_cast<int>(o.rank);
    }
    return kickers < o.kickers;
}

bool HandEvaluation::operator==(const HandEvaluation& o) const {
    return rank == o.rank && kickers == o.kickers;
}

HandEvaluation evaluate_five_cards(const std::vector<Card>& five) {
    if (five.size() != 5) {
        return HandEvaluation{};
    }
    return evaluate_sorted_five(sort_cards_desc(five));
}

HandEvaluation evaluate_best_hand(const std::vector<Card>& cards) {
    if (cards.empty()) {
        return HandEvaluation{};
    }
    if (cards.size() < 5) {
        return partial_high_card(cards);
    }
    HandEvaluation best{};
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
        std::vector<Card> five;
        for (int k : idx) {
            five.push_back(cards[static_cast<std::size_t>(k)]);
        }
        HandEvaluation ev = evaluate_five_cards(five);
        if (!init || best < ev) {
            best = ev;
            init = true;
        }
    } while (bump());
    return best;
}

HandRank hand_category(const HandEvaluation& e) { return e.rank; }

std::uint64_t evaluate_hand_strength(const std::vector<Card>& player_hand,
                                       const std::vector<Card>& community_cards) {
    std::vector<Card> all = player_hand;
    all.insert(all.end(), community_cards.begin(), community_cards.end());
    HandEvaluation e = evaluate_best_hand(all);
    std::uint64_t x = static_cast<std::uint64_t>(e.rank) << 24;
    for (int i = 0; i < 5; ++i) {
        x |= static_cast<std::uint64_t>(e.kickers[static_cast<std::size_t>(i)] & 0x1F)
             << (4 * (4 - i));
    }
    return x;
}

HandRank evaluate_hand(const std::vector<Card>& hand, const std::vector<Card>& community_cards) {
    std::vector<Card> all = hand;
    all.insert(all.end(), community_cards.begin(), community_cards.end());
    return evaluate_best_hand(all).rank;
}

int compare_best_hands(const std::vector<Card>& a, const std::vector<Card>& b) {
    if (a.empty() || a.size() > 7 || b.empty() || b.size() > 7) {
        throw std::invalid_argument("compareBestHands: each side needs 1..7 cards");
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = i + 1; j < a.size(); ++j) {
            if (a[i] == a[j]) {
                throw std::invalid_argument("compareBestHands: duplicate card in cardsA");
            }
        }
    }
    for (std::size_t i = 0; i < b.size(); ++i) {
        for (std::size_t j = i + 1; j < b.size(); ++j) {
            if (b[i] == b[j]) {
                throw std::invalid_argument("compareBestHands: duplicate card in cardsB");
            }
        }
    }
    for (const auto& ca : a) {
        for (const auto& cb : b) {
            if (ca == cb) {
                throw std::invalid_argument("compareBestHands: overlapping cards between hands");
            }
        }
    }
    const HandEvaluation ea = evaluate_best_hand(a);
    const HandEvaluation eb = evaluate_best_hand(b);
    if (ea < eb) {
        return -1;
    }
    if (eb < ea) {
        return 1;
    }
    return 0;
}

}  // namespace poker
