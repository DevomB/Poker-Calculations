#include "poker/equity_matrix.hpp"

#include "poker/card_string.hpp"
#include "poker/monte_carlo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

namespace poker {

namespace {

bool hand169_to_ranks_suited(int hand169, int& high_rank, int& low_rank, bool& suited) {
    if (hand169 < 0 || hand169 >= kHandClassCount) {
        return false;
    }
    const HandClass169 c = hand169_class(hand169);
    high_rank = c.high_rank;
    low_rank = c.low_rank;
    suited = c.suited;
    return true;
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

int hand169_index(int high_rank, int low_rank, bool suited) {
    if (high_rank < 0 || high_rank > 12 || low_rank < 0 || low_rank > 12) {
        throw std::invalid_argument("rank out of range");
    }
    const int high = std::max(high_rank, low_rank);
    const int low = std::min(high_rank, low_rank);
    if (high == low && suited) {
        throw std::invalid_argument("pairs have no suited/offsuit suffix");
    }
    // Block `low` starts after the blocks of lower ranks: sum over r < low of 1 + 2 (12 - r).
    const int block = low * (26 - low);
    return high == low ? block : block + 1 + 2 * (high - low - 1) + (suited ? 0 : 1);
}

HandClass169 hand169_class(int hand169) {
    if (hand169 < 0 || hand169 >= kHandClassCount) {
        throw std::invalid_argument("hand169 must be in 0..168");
    }
    int low = 0;
    while (hand169 >= (low + 1) * (25 - low)) {
        ++low;
    }
    const int offset = hand169 - low * (26 - low);
    if (offset == 0) {
        return HandClass169{low, low, false};
    }
    return HandClass169{low + 1 + (offset - 1) / 2, low, (offset - 1) % 2 == 0};
}

bool hand169_to_deck_indices(int hand169, int& c0, int& c1) {
    return pick_disjoint_holes(hand169, c0, c1);
}

void build_preflop_equity_matrix(const PreflopMatrixOptions& opts, std::vector<double>& out) {
    if (opts.iterations < 1) {
        throw std::invalid_argument("iterations must be positive");
    }
    out.assign(169 * 169, 0.0);
    const int iters = opts.iterations;
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
