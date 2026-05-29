#include "poker/equity_matrix.hpp"

#include "poker/card_string.hpp"
#include "poker/monte_carlo.hpp"

#include <array>
#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

namespace poker {

namespace {

bool hand169_to_ranks_suited(int hand169, int& high_rank, int& low_rank, bool& suited) {
    if (hand169 < 0 || hand169 > 168) {
        return false;
    }
    int idx = 0;
    for (int i = 0; i < 13; ++i) {
        for (int j = i; j < 13; ++j) {
            if (i == j) {
                if (idx == hand169) {
                    high_rank = i;
                    low_rank = j;
                    suited = false;
                    return true;
                }
                ++idx;
            } else {
                if (idx == hand169) {
                    high_rank = j;
                    low_rank = i;
                    suited = true;
                    return true;
                }
                ++idx;
                if (idx == hand169) {
                    high_rank = j;
                    low_rank = i;
                    suited = false;
                    return true;
                }
                ++idx;
            }
        }
    }
    return false;
}

bool pick_disjoint_holes(int hand169, int& c0, int& c1) {
    int hr = 0;
    int lr = 0;
    bool suited = false;
    if (!hand169_to_ranks_suited(hand169, hr, lr, suited)) {
        return false;
    }
    for (int s_high = 0; s_high < 4; ++s_high) {
        for (int s_low = 0; s_low < 4; ++s_low) {
            if (suited && s_low != s_high) {
                continue;
            }
            if (!suited && s_low == s_high) {
                continue;
            }
            const int i0 = hr * 4 + s_high;
            const int i1 = lr * 4 + (suited ? s_high : s_low);
            if (i0 != i1) {
                c0 = i0;
                c1 = i1;
                return true;
            }
        }
    }
    return false;
}

bool pick_disjoint_matchup(int hero169, int vill169, int& h0, int& h1, int& v0, int& v1) {
    if (!pick_disjoint_holes(hero169, h0, h1)) {
        return false;
    }
    int hr = 0;
    int lr = 0;
    bool suited = false;
    if (!hand169_to_ranks_suited(vill169, hr, lr, suited)) {
        return false;
    }
    for (int s_high = 0; s_high < 4; ++s_high) {
        for (int s_low = 0; s_low < 4; ++s_low) {
            if (suited && s_low != s_high) {
                continue;
            }
            if (!suited && s_low == s_high) {
                continue;
            }
            const int i0 = hr * 4 + s_high;
            const int i1 = lr * 4 + (suited ? s_high : s_low);
            if (i0 == i1) {
                continue;
            }
            if (i0 != h0 && i0 != h1 && i1 != h0 && i1 != h1) {
                v0 = i0;
                v1 = i1;
                return true;
            }
        }
    }
    return false;
}

}  // namespace

bool hand169_to_deck_indices(int hand169, int& c0, int& c1) {
    return pick_disjoint_holes(hand169, c0, c1);
}

void build_preflop_equity_matrix(const PreflopMatrixOptions& opts, std::vector<double>& out) {
    if (opts.iterations < 1) {
        throw std::invalid_argument("iterations must be positive");
    }
    out.assign(169 * 169, 0.0);
    const int iters = opts.iterations;
    std::size_t threads = opts.num_threads;
    if (threads == 0) {
        threads = 1;
    }
    const std::vector<Card> empty_board;

    for (int i = 0; i < 169; ++i) {
        int hi0 = 0;
        int hi1 = 0;
        if (!pick_disjoint_holes(i, hi0, hi1)) {
            continue;
        }
        const std::vector<Card> hero = {card_from_deck_index(hi0), card_from_deck_index(hi1)};

        for (int j = i; j < 169; ++j) {
            double e = 0.5;
            if (i == j) {
                e = 0.5;
            } else {
                int v0 = 0;
                int v1 = 0;
                if (pick_disjoint_matchup(i, j, hi0, hi1, v0, v1)) {
                    const std::uint32_t seed =
                        opts.seed + static_cast<std::uint32_t>(i * 617 + j * 991);
                    std::mt19937 rng(seed);
                    const float eq = simulate_hand_outcome_vs_villain_holes(
                        hero, empty_board, v0, v1, iters, rng);
                    e = static_cast<double>(eq);
                }
            }
            out[static_cast<std::size_t>(i * 169 + j)] = e;
            out[static_cast<std::size_t>(j * 169 + i)] = (i == j) ? 0.5 : (1.0 - e);
        }
    }
}

}  // namespace poker
