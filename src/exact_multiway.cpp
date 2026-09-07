#include "poker/exact_multiway.hpp"

#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/side_pot.hpp"

#include <array>
#include <stdexcept>

namespace poker {
namespace {

constexpr int kMinPlayers = 3;
constexpr int kMaxPlayers = 6;

struct MultiwaySpot {
    int n{0};
    std::array<std::array<int, 2>, kMaxPlayers> hole_idx{};
    std::vector<Card> board;
    std::vector<int> remaining;
    int need{0};
};

void mark_or_throw_duplicate(DeckBitset& used, const std::vector<Card>& cards, const char* where) {
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (used.test(idx)) {
            throw std::invalid_argument(std::string("duplicate card in ") + where);
        }
        used.set(idx);
    }
}

[[nodiscard]] std::uint64_t binomial_uint64(int n, int k) {
    if (k < 0 || n < 0 || k > n) {
        return 0;
    }
    if (k == 0 || k == n) {
        return 1;
    }
    if (k > n - k) {
        k = n - k;
    }
    std::uint64_t acc = 1;
    for (int i = 1; i <= k; ++i) {
        acc = acc * static_cast<std::uint64_t>(n - k + i) / static_cast<std::uint64_t>(i);
    }
    return acc;
}

MultiwaySpot prepare_multiway_spot(const std::vector<std::vector<Card>>& hole_hands,
                                   const std::vector<Card>& board_cards,
                                   const std::vector<Card>& dead_cards) {
    const int n = static_cast<int>(hole_hands.size());
    if (n < kMinPlayers || n > kMaxPlayers) {
        throw std::invalid_argument("exact multiway requires 3 to 6 known hands");
    }
    if (board_cards.size() > 5) {
        throw std::invalid_argument("board must have at most 5 cards");
    }
    DeckBitset used;
    for (int i = 0; i < n; ++i) {
        if (hole_hands[static_cast<std::size_t>(i)].size() != 2) {
            throw std::invalid_argument("each player must have exactly two hole cards");
        }
        mark_or_throw_duplicate(used, hole_hands[static_cast<std::size_t>(i)], "hole cards");
    }
    mark_or_throw_duplicate(used, board_cards, "board");
    mark_or_throw_duplicate(used, dead_cards, "dead cards");

    MultiwaySpot spot;
    spot.n = n;
    spot.board = board_cards;
    for (int i = 0; i < n; ++i) {
        const auto& hole = hole_hands[static_cast<std::size_t>(i)];
        spot.hole_idx[static_cast<std::size_t>(i)][0] = deck_index_from_card(hole[0]);
        spot.hole_idx[static_cast<std::size_t>(i)][1] = deck_index_from_card(hole[1]);
    }
    spot.need = 5 - static_cast<int>(board_cards.size());
    spot.remaining = used.unused_indices();
    if (static_cast<int>(spot.remaining.size()) < spot.need) {
        throw std::invalid_argument("not enough unknown cards for enumeration");
    }
    return spot;
}

void fill_seven_from_indices(int h0, int h1, const std::vector<Card>& board, const int* run, int run_k,
                             std::uint8_t* ranks, std::uint8_t* suits) {
    ranks[0] = static_cast<std::uint8_t>(h0 / 4);
    suits[0] = static_cast<std::uint8_t>(h0 % 4);
    ranks[1] = static_cast<std::uint8_t>(h1 / 4);
    suits[1] = static_cast<std::uint8_t>(h1 % 4);
    const std::size_t board_n = board.size();
    for (std::size_t bi = 0; bi < board_n; ++bi) {
        ranks[2 + bi] = board[bi].rank();
        suits[2 + bi] = board[bi].suit();
    }
    const std::size_t base = 2 + board_n;
    for (int ri = 0; ri < run_k; ++ri) {
        const int idx = run[ri];
        ranks[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx / 4);
        suits[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx % 4);
    }
}

void evaluate_showdown_strengths(const MultiwaySpot& spot, const int* run, int run_k,
                                 std::uint64_t* strengths) {
    std::uint8_t ranks[7]{};
    std::uint8_t suits[7]{};
    for (int i = 0; i < spot.n; ++i) {
        fill_seven_from_indices(spot.hole_idx[static_cast<std::size_t>(i)][0],
                                spot.hole_idx[static_cast<std::size_t>(i)][1], spot.board, run, run_k,
                                ranks, suits);
        strengths[i] = evaluate_seven_strength_fast(ranks, suits);
    }
}

template <typename Fn>
void for_each_multiway_showdown(const MultiwaySpot& spot, Fn&& fn) {
    std::uint64_t strengths[kMaxPlayers]{};
    if (spot.need == 0) {
        evaluate_showdown_strengths(spot, nullptr, 0, strengths);
        fn(strengths);
        return;
    }
    for_each_combo_indices(spot.remaining, spot.need, [&](const int* run, int run_k) {
        evaluate_showdown_strengths(spot, run, run_k, strengths);
        fn(strengths);
    });
}

void pot_shares_from_strengths(const std::uint64_t* strengths, int n, const bool* eligible,
                               double* shares) {
    std::uint64_t best = 0;
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (!eligible[i]) {
            continue;
        }
        if (!any || strengths[i] > best) {
            best = strengths[i];
            any = true;
        }
    }
    int tied = 0;
    for (int i = 0; i < n; ++i) {
        shares[i] = 0.0;
        if (eligible[i] && any && strengths[i] == best) {
            ++tied;
        }
    }
    if (tied == 0) {
        return;
    }
    const double part = 1.0 / static_cast<double>(tied);
    for (int i = 0; i < n; ++i) {
        if (eligible[i] && strengths[i] == best) {
            shares[i] = part;
        }
    }
}

void all_eligible(int n, bool* eligible) {
    for (int i = 0; i < n; ++i) {
        eligible[i] = true;
    }
}

}  // namespace

std::vector<double> exact_multiway_equity_known_hands(const std::vector<std::vector<Card>>& hole_hands,
                                                      const std::vector<Card>& board_cards,
                                                      const std::vector<Card>& dead_cards) {
    const MultiwaySpot spot = prepare_multiway_spot(hole_hands, board_cards, dead_cards);
    std::vector<double> eq(static_cast<std::size_t>(spot.n), 0.0);
    double total = 0.0;
    bool eligible[kMaxPlayers]{};
    all_eligible(spot.n, eligible);
    double shares[kMaxPlayers]{};
    for_each_multiway_showdown(spot, [&](const std::uint64_t* strengths) {
        pot_shares_from_strengths(strengths, spot.n, eligible, shares);
        for (int i = 0; i < spot.n; ++i) {
            eq[static_cast<std::size_t>(i)] += shares[i];
        }
        total += 1.0;
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exact multiway: empty enumeration");
    }
    for (double& e : eq) {
        e /= total;
    }
    return eq;
}

std::vector<double> exact_three_way_equity_known_hands(const std::vector<Card>& hole0,
                                                       const std::vector<Card>& hole1,
                                                       const std::vector<Card>& hole2,
                                                       const std::vector<Card>& board_cards,
                                                       const std::vector<Card>& dead_cards) {
    return exact_multiway_equity_known_hands({hole0, hole1, hole2}, board_cards, dead_cards);
}

std::vector<double> exact_four_way_equity_known_hands(const std::vector<Card>& hole0,
                                                      const std::vector<Card>& hole1,
                                                      const std::vector<Card>& hole2,
                                                      const std::vector<Card>& hole3,
                                                      const std::vector<Card>& board_cards,
                                                      const std::vector<Card>& dead_cards) {
    return exact_multiway_equity_known_hands({hole0, hole1, hole2, hole3}, board_cards, dead_cards);
}

MultiwayWinTieLose exact_multiway_win_tie_lose_known_hands(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards) {
    const MultiwaySpot spot = prepare_multiway_spot(hole_hands, board_cards, dead_cards);
    MultiwayWinTieLose out;
    out.win.assign(static_cast<std::size_t>(spot.n), 0.0);
    out.split.assign(static_cast<std::size_t>(spot.n), 0.0);
    out.lose.assign(static_cast<std::size_t>(spot.n), 0.0);
    double total = 0.0;
    for_each_multiway_showdown(spot, [&](const std::uint64_t* strengths) {
        std::uint64_t best = strengths[0];
        for (int i = 1; i < spot.n; ++i) {
            if (strengths[i] > best) {
                best = strengths[i];
            }
        }
        int tied = 0;
        for (int i = 0; i < spot.n; ++i) {
            if (strengths[i] == best) {
                ++tied;
            }
        }
        for (int i = 0; i < spot.n; ++i) {
            if (strengths[i] != best) {
                out.lose[static_cast<std::size_t>(i)] += 1.0;
            } else if (tied == 1) {
                out.win[static_cast<std::size_t>(i)] += 1.0;
            } else {
                out.split[static_cast<std::size_t>(i)] += 1.0;
            }
        }
        total += 1.0;
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exact multiway: empty enumeration");
    }
    for (int i = 0; i < spot.n; ++i) {
        out.win[static_cast<std::size_t>(i)] /= total;
        out.split[static_cast<std::size_t>(i)] /= total;
        out.lose[static_cast<std::size_t>(i)] /= total;
    }
    return out;
}

MultiwayWinTieLose exact_three_way_win_tie_lose_known_hands(
    const std::vector<Card>& hole0, const std::vector<Card>& hole1, const std::vector<Card>& hole2,
    const std::vector<Card>& board_cards, const std::vector<Card>& dead_cards) {
    return exact_multiway_win_tie_lose_known_hands({hole0, hole1, hole2}, board_cards, dead_cards);
}

MultiwaySidePotChipEv exact_multiway_side_pot_chip_ev(const std::vector<double>& committed_chips,
                                                      const std::vector<std::vector<Card>>& hole_hands,
                                                      const std::vector<Card>& board_cards,
                                                      const std::vector<Card>& dead_cards) {
    const MultiwaySpot spot = prepare_multiway_spot(hole_hands, board_cards, dead_cards);
    if (static_cast<int>(committed_chips.size()) != spot.n) {
        throw std::invalid_argument("commitments must have one entry per player");
    }
    const std::vector<Side_pot_layer> layers = side_pot_ladder_from_commitments(committed_chips);
    MultiwaySidePotChipEv out;
    out.chip_ev.assign(static_cast<std::size_t>(spot.n), 0.0);
    out.layer_count = static_cast<int>(layers.size());
    if (layers.empty()) {
        return out;
    }
    double total = 0.0;
    for_each_multiway_showdown(spot, [&](const std::uint64_t* strengths) {
        for (const Side_pot_layer& layer : layers) {
            bool eligible[kMaxPlayers]{};
            for (int i = 0; i < spot.n; ++i) {
                eligible[i] = layer.player_cap_contribution[static_cast<std::size_t>(i)] > 0.0;
            }
            double shares[kMaxPlayers]{};
            pot_shares_from_strengths(strengths, spot.n, eligible, shares);
            for (int i = 0; i < spot.n; ++i) {
                out.chip_ev[static_cast<std::size_t>(i)] += layer.pot_chips * shares[i];
            }
        }
        total += 1.0;
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exact multiway: empty enumeration");
    }
    for (double& ev : out.chip_ev) {
        ev /= total;
    }
    return out;
}

MultiwayAheadFrequency exact_multiway_ahead_frequency(const std::vector<std::vector<Card>>& hole_hands,
                                                      const std::vector<Card>& board_cards,
                                                      const std::vector<Card>& dead_cards) {
    if (board_cards.size() != 3 && board_cards.size() != 4) {
        throw std::invalid_argument("exactMultiwayAheadFrequency requires a flop or turn board");
    }
    const std::vector<double> eq =
        exact_multiway_equity_known_hands(hole_hands, board_cards, dead_cards);
    MultiwayAheadFrequency out;
    out.p_win_showdown = eq[0];

    std::uint64_t now[kMaxPlayers]{};
    const int n = static_cast<int>(hole_hands.size());
    for (int i = 0; i < n; ++i) {
        now[i] = evaluate_hand_strength_fast(hole_hands[static_cast<std::size_t>(i)], board_cards);
    }
    bool eligible[kMaxPlayers]{};
    all_eligible(n, eligible);
    double shares[kMaxPlayers]{};
    pot_shares_from_strengths(now, n, eligible, shares);
    out.p_ahead_now = shares[0];
    return out;
}

MultiwayTieFrequency exact_multiway_tie_frequency(const std::vector<std::vector<Card>>& hole_hands,
                                                  const std::vector<Card>& board_cards,
                                                  const std::vector<Card>& dead_cards) {
    const MultiwaySpot spot = prepare_multiway_spot(hole_hands, board_cards, dead_cards);
    double hero_split = 0.0;
    double any_split = 0.0;
    double total = 0.0;
    for_each_multiway_showdown(spot, [&](const std::uint64_t* strengths) {
        std::uint64_t best = strengths[0];
        for (int i = 1; i < spot.n; ++i) {
            if (strengths[i] > best) {
                best = strengths[i];
            }
        }
        int tied = 0;
        for (int i = 0; i < spot.n; ++i) {
            if (strengths[i] == best) {
                ++tied;
            }
        }
        if (tied >= 2) {
            any_split += 1.0;
            if (strengths[0] == best) {
                hero_split += 1.0;
            }
        }
        total += 1.0;
    });
    if (total <= 0.0) {
        throw std::invalid_argument("exact multiway: empty enumeration");
    }
    return MultiwayTieFrequency{hero_split / total, any_split / total};
}

std::uint64_t exact_multiway_runout_count(const std::vector<std::vector<Card>>& hole_hands,
                                          const std::vector<Card>& board_cards,
                                          const std::vector<Card>& dead_cards) {
    const MultiwaySpot spot = prepare_multiway_spot(hole_hands, board_cards, dead_cards);
    if (spot.need == 0) {
        return 1;
    }
    return binomial_uint64(static_cast<int>(spot.remaining.size()), spot.need);
}

MultiwayBestWorstRunout exact_multiway_best_worst_runout(
    const std::vector<std::vector<Card>>& hole_hands, const std::vector<Card>& board_cards,
    const std::vector<Card>& dead_cards) {
    MultiwayBestWorstRunout out;
    if (board_cards.size() != 3 && board_cards.size() != 4) {
        return out;
    }
    const MultiwaySpot spot = prepare_multiway_spot(hole_hands, board_cards, dead_cards);
    if (spot.remaining.empty()) {
        return out;
    }
    bool have = false;
    for (int idx : spot.remaining) {
        std::vector<Card> next_board = board_cards;
        next_board.push_back(card_from_deck_index(idx));
        const std::vector<double> eq =
            exact_multiway_equity_known_hands(hole_hands, next_board, dead_cards);
        const double hero = eq[0];
        const Card card = card_from_deck_index(idx);
        if (!have || hero > out.best_equity) {
            out.best_equity = hero;
            out.best_card = card;
        }
        if (!have || hero < out.worst_equity) {
            out.worst_equity = hero;
            out.worst_card = card;
        }
        have = true;
    }
    out.supported = have;
    return out;
}

}  // namespace poker
