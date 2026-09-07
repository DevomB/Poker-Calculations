#include "poker/suit_isomorphism.hpp"

#include "poker/card_string.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace poker {
namespace {

constexpr int kDeck = 52;

[[nodiscard]] int apply_idx(int idx, const SuitPerm& perm) {
    return (idx / 4) * 4 + perm[static_cast<std::size_t>(idx % 4)];
}

[[nodiscard]] int combo_index_1326(int a, int b) {
    if (a > b) {
        std::swap(a, b);
    }
    return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

struct IsoScore {
    std::array<int, 7> cards{-1, -1, -1, -1, -1, -1, -1};
    SuitPerm perm{0, 1, 2, 3};

    friend bool operator<(const IsoScore& a, const IsoScore& b) {
        if (a.cards != b.cards) {
            return a.cards < b.cards;
        }
        return a.perm < b.perm;
    }
};

[[nodiscard]] std::array<int, 3> sorted_flop_ids(const std::vector<Card>& flop, const SuitPerm& perm) {
    std::array<int, 3> ids{
        apply_idx(deck_index_from_card(flop[0]), perm),
        apply_idx(deck_index_from_card(flop[1]), perm),
        apply_idx(deck_index_from_card(flop[2]), perm),
    };
    std::sort(ids.begin(), ids.end());
    return ids;
}

void require_unique_cards(const std::vector<Card>& cards, const char* ctx) {
    if (cards_have_duplicate(cards)) {
        throw std::invalid_argument(std::string(ctx) + ": duplicate cards");
    }
}

void require_flop(const std::vector<Card>& flop) {
    if (flop.size() != 3) {
        throw std::invalid_argument("canonical flop requires exactly 3 cards");
    }
    require_unique_cards(flop, "flop");
}

void require_board(const std::vector<Card>& board) {
    if (board.size() < 3 || board.size() > 5) {
        throw std::invalid_argument("board must have 3, 4, or 5 cards");
    }
    require_unique_cards(board, "board");
}

[[nodiscard]] IsoScore score_board(const std::vector<Card>& board, const SuitPerm& perm) {
    IsoScore s;
    s.perm = perm;
    const auto flop = sorted_flop_ids(board, perm);
    s.cards[0] = flop[0];
    s.cards[1] = flop[1];
    s.cards[2] = flop[2];
    if (board.size() >= 4) {
        s.cards[3] = apply_idx(deck_index_from_card(board[3]), perm);
    }
    if (board.size() >= 5) {
        s.cards[4] = apply_idx(deck_index_from_card(board[4]), perm);
    }
    return s;
}

[[nodiscard]] IsoScore score_holes_and_board(const std::vector<Card>& holes, const std::vector<Card>& board,
                                             const SuitPerm& perm) {
    IsoScore s = score_board(board, perm);
    std::array<int, 2> h{
        apply_idx(deck_index_from_card(holes[0]), perm),
        apply_idx(deck_index_from_card(holes[1]), perm),
    };
    std::sort(h.begin(), h.end());
    s.cards[5] = h[0];
    s.cards[6] = h[1];
    return s;
}

template <typename MakeScore>
[[nodiscard]] SuitPerm best_suit_perm(MakeScore&& make_score) {
    SuitPerm perm{0, 1, 2, 3};
    IsoScore best{};
    bool first = true;
    do {
        IsoScore s = make_score(perm);
        s.perm = perm;
        if (first || s < best) {
            best = s;
            first = false;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
    return best.perm;
}

[[nodiscard]] std::vector<Card> cards_from_ids(const int* ids, std::size_t n) {
    std::vector<Card> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(card_from_deck_index(ids[i]));
    }
    return out;
}

[[nodiscard]] std::uint32_t pack_flop_key(int a, int b, int c) {
    return (static_cast<std::uint32_t>(a) << 16) | (static_cast<std::uint32_t>(b) << 8) |
           static_cast<std::uint32_t>(c);
}

struct FlopIsoTables {
    std::vector<std::uint32_t> keys;
};

const FlopIsoTables& flop_iso_tables() {
    static const FlopIsoTables tables = [] {
        FlopIsoTables t;
        std::set<std::uint32_t> uniq;
        for (int a = 0; a < kDeck; ++a) {
            for (int b = a + 1; b < kDeck; ++b) {
                for (int c = b + 1; c < kDeck; ++c) {
                    const std::vector<Card> flop{card_from_deck_index(a), card_from_deck_index(b),
                                                 card_from_deck_index(c)};
                    const SuitPerm perm = best_suit_perm(
                        [&](const SuitPerm& p) { return score_board(flop, p); });
                    const auto ids = sorted_flop_ids(flop, perm);
                    uniq.insert(pack_flop_key(ids[0], ids[1], ids[2]));
                }
            }
        }
        t.keys.assign(uniq.begin(), uniq.end());
        if (static_cast<int>(t.keys.size()) != kCanonicalFlopCount) {
            throw std::logic_error("suit isomorphism: expected 1755 canonical flops, got " +
                                   std::to_string(t.keys.size()));
        }
        return t;
    }();
    return tables;
}

[[nodiscard]] std::uint32_t canonical_flop_key(const std::vector<Card>& flop) {
    const SuitPerm perm = suit_perm_from_canonical_flop(flop);
    const auto ids = sorted_flop_ids(flop, perm);
    return pack_flop_key(ids[0], ids[1], ids[2]);
}

}  // namespace

bool is_valid_suit_perm(const SuitPerm& perm) {
    std::array<bool, kSuitCount> seen{};
    for (int s : perm) {
        if (s < 0 || s >= kSuitCount || seen[static_cast<std::size_t>(s)]) {
            return false;
        }
        seen[static_cast<std::size_t>(s)] = true;
    }
    return true;
}

std::vector<Card> apply_suit_perm_to_cards(const std::vector<Card>& cards, const SuitPerm& perm) {
    if (!is_valid_suit_perm(perm)) {
        throw std::invalid_argument("suit perm must be a bijection of 0..3");
    }
    std::vector<Card> out;
    out.reserve(cards.size());
    for (const Card& c : cards) {
        out.push_back(card_from_deck_index(apply_idx(deck_index_from_card(c), perm)));
    }
    return out;
}

SuitPerm suit_perm_from_canonical_flop(const std::vector<Card>& flop) {
    require_flop(flop);
    return best_suit_perm([&](const SuitPerm& p) { return score_board(flop, p); });
}

std::vector<Card> canonical_flop_board(const std::vector<Card>& flop) {
    const SuitPerm perm = suit_perm_from_canonical_flop(flop);
    const auto ids = sorted_flop_ids(flop, perm);
    return cards_from_ids(ids.data(), 3);
}

std::vector<Card> canonical_board(const std::vector<Card>& board) {
    require_board(board);
    const SuitPerm perm = best_suit_perm([&](const SuitPerm& p) { return score_board(board, p); });
    std::vector<Card> out = apply_suit_perm_to_cards(board, perm);
    std::sort(out.begin(), out.begin() + 3, [](const Card& a, const Card& b) {
        return deck_index_from_card(a) < deck_index_from_card(b);
    });
    return out;
}

CanonicalHolesAndBoard canonical_holes_and_board(const std::vector<Card>& holes,
                                                 const std::vector<Card>& board) {
    if (holes.size() != 2) {
        throw std::invalid_argument("hero holes require exactly 2 cards");
    }
    require_unique_cards(holes, "holes");
    require_board(board);
    std::vector<Card> all = holes;
    all.insert(all.end(), board.begin(), board.end());
    require_unique_cards(all, "holes+board");
    const SuitPerm perm =
        best_suit_perm([&](const SuitPerm& p) { return score_holes_and_board(holes, board, p); });
    CanonicalHolesAndBoard out;
    out.perm = perm;
    out.holes = apply_suit_perm_to_cards(holes, perm);
    std::sort(out.holes.begin(), out.holes.end(), [](const Card& a, const Card& b) {
        return deck_index_from_card(a) < deck_index_from_card(b);
    });
    out.board = apply_suit_perm_to_cards(board, perm);
    std::sort(out.board.begin(), out.board.begin() + 3, [](const Card& a, const Card& b) {
        return deck_index_from_card(a) < deck_index_from_card(b);
    });
    return out;
}

std::vector<double> apply_suit_perm_to_range1326(const double* weights, std::size_t len,
                                                 const SuitPerm& perm) {
    if (weights == nullptr || len != static_cast<std::size_t>(kCombo1326)) {
        throw std::invalid_argument("dense range must have length 1326");
    }
    if (!is_valid_suit_perm(perm)) {
        throw std::invalid_argument("suit perm must be a bijection of 0..3");
    }
    std::vector<double> out(static_cast<std::size_t>(kCombo1326), 0.0);
    int n = 0;
    for (int a = 0; a < kDeck; ++a) {
        for (int b = a + 1; b < kDeck; ++b) {
            const int dest = combo_index_1326(apply_idx(a, perm), apply_idx(b, perm));
            out[static_cast<std::size_t>(dest)] = weights[static_cast<std::size_t>(n)];
            ++n;
        }
    }
    return out;
}

int isomorphic_flop_orbit_size(const std::vector<Card>& flop) {
    require_flop(flop);
    std::set<std::uint32_t> orbit;
    SuitPerm perm{0, 1, 2, 3};
    do {
        auto ids = sorted_flop_ids(flop, perm);
        orbit.insert(pack_flop_key(ids[0], ids[1], ids[2]));
    } while (std::next_permutation(perm.begin(), perm.end()));
    return static_cast<int>(orbit.size());
}

int count_canonical_flops() {
    return static_cast<int>(flop_iso_tables().keys.size());
}

int isomorphic_flop_index(const std::vector<Card>& flop) {
    const std::uint32_t key = canonical_flop_key(flop);
    const auto& keys = flop_iso_tables().keys;
    const auto it = std::lower_bound(keys.begin(), keys.end(), key);
    if (it == keys.end() || *it != key) {
        throw std::logic_error("canonical flop key missing from 1755 table");
    }
    return static_cast<int>(it - keys.begin());
}

std::vector<Card> flop_index_to_canonical(int index) {
    if (index < 0 || index >= kCanonicalFlopCount) {
        throw std::invalid_argument("flop index must be 0..1754");
    }
    const std::uint32_t key = flop_iso_tables().keys[static_cast<std::size_t>(index)];
    const int ids[3] = {static_cast<int>((key >> 16) & 0xff), static_cast<int>((key >> 8) & 0xff),
                        static_cast<int>(key & 0xff)};
    return cards_from_ids(ids, 3);
}

}  // namespace poker
