#include "poker/hand_potential.hpp"

#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

namespace poker {

namespace {

constexpr int kAhead = 0;
constexpr int kTied = 1;
constexpr int kBehind = 2;
constexpr int kCancelStride = 4096;

int outcome_of(std::uint64_t hero, std::uint64_t villain) {
    if (hero > villain) {
        return kAhead;
    }
    if (hero == villain) {
        return kTied;
    }
    return kBehind;
}

std::uint64_t strength_ids(int h0, int h1, const int* board, int board_n, const int* extra,
                           int extra_n) {
    std::uint8_t ranks[7]{};
    std::uint8_t suits[7]{};
    ranks[0] = static_cast<std::uint8_t>(h0 / 4);
    suits[0] = static_cast<std::uint8_t>(h0 % 4);
    ranks[1] = static_cast<std::uint8_t>(h1 / 4);
    suits[1] = static_cast<std::uint8_t>(h1 % 4);
    for (int i = 0; i < board_n; ++i) {
        ranks[static_cast<std::size_t>(2 + i)] = static_cast<std::uint8_t>(board[i] / 4);
        suits[static_cast<std::size_t>(2 + i)] = static_cast<std::uint8_t>(board[i] % 4);
    }
    for (int i = 0; i < extra_n; ++i) {
        ranks[static_cast<std::size_t>(2 + board_n + i)] = static_cast<std::uint8_t>(extra[i] / 4);
        suits[static_cast<std::size_t>(2 + board_n + i)] = static_cast<std::uint8_t>(extra[i] % 4);
    }
    return pack_hand_strength(evaluate_best_hand_fast(ranks, suits, 2 + board_n + extra_n));
}

void validate_hero_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                         PotentialStreets streets) {
    if (hero.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (hero[0] == hero[1]) {
        throw std::invalid_argument("hero hole cards must be distinct");
    }
    const std::size_t n = board.size();
    if (streets == PotentialStreets::Two) {
        if (n != 3) {
            throw std::invalid_argument("two-street potential requires a flop (3 board cards)");
        }
    } else if (n != 3 && n != 4) {
        throw std::invalid_argument("hand potential requires a flop or turn (3 or 4 board cards)");
    }
    DeckBitset used;
    used.mark_cards(hero);
    for (const Card& c : board) {
        const int idx = deck_index_from_card(c);
        if (used.test(idx)) {
            throw std::invalid_argument("duplicate card in hero or board");
        }
        used.set(idx);
    }
}

void finish_breakdown(HandPotentialBreakdown& out) {
    const double total = out.n_ahead + out.n_tied + out.n_behind;
    out.hs = total > 0.0 ? (out.n_ahead + 0.5 * out.n_tied) / total : 0.0;
    const double behind_mass = out.n_behind + 0.5 * out.n_tied;
    const double ahead_mass = out.n_ahead + 0.5 * out.n_tied;
    out.ppot = behind_mass > 0.0 ? out.ppot / behind_mass : 0.0;
    out.npot = ahead_mass > 0.0 ? out.npot / ahead_mass : 0.0;
    out.ehs = out.hs * (1.0 - out.npot) + (1.0 - out.hs) * out.ppot;
    out.ehs2 = out.hs * (1.0 - out.npot) * (1.0 - out.npot) + (1.0 - out.hs) * out.ppot * out.ppot;
}

void add_transition(double hp[3][3], double weight, int now, int later) {
    hp[now][later] += weight;
}

void apply_transitions(HandPotentialBreakdown& out, const double hp[3][3]) {
    // Billings / poker-eval: half-chop on ties in both numerator and denominator.
    out.ppot = hp[kBehind][kAhead] + 0.5 * hp[kBehind][kTied] + 0.5 * hp[kTied][kAhead];
    out.npot = hp[kAhead][kBehind] + 0.5 * hp[kAhead][kTied] + 0.5 * hp[kTied][kBehind];
}

void accumulate_exact(int h0, int h1, const int* board, int board_n, int need,
                      const SparseRange& range, std::uint64_t hero_board_mask,
                      HandPotentialBreakdown& out, const CancelPredicate* cancel) {
    double hp[3][3]{};
    std::size_t steps = 0;
    const std::uint64_t hero_now = strength_ids(h0, h1, board, board_n, nullptr, 0);
    for (const WeightedHoleCombo& combo : range.combos) {
        const std::uint64_t vil_mask =
            (std::uint64_t{1} << combo.card_a) | (std::uint64_t{1} << combo.card_b);
        if ((hero_board_mask & vil_mask) != 0) {
            continue;
        }
        const std::uint64_t vil_now =
            strength_ids(combo.card_a, combo.card_b, board, board_n, nullptr, 0);
        const int now = outcome_of(hero_now, vil_now);
        if (now == kAhead) {
            out.n_ahead += combo.weight;
        } else if (now == kTied) {
            out.n_tied += combo.weight;
        } else {
            out.n_behind += combo.weight;
        }

        std::vector<int> live;
        live.reserve(52);
        const std::uint64_t dead = hero_board_mask | vil_mask;
        for (int i = 0; i < 52; ++i) {
            if ((dead & (std::uint64_t{1} << i)) == 0) {
                live.push_back(i);
            }
        }
        if (static_cast<int>(live.size()) < need) {
            continue;
        }

        double combo_hp[3][3]{};
        int runouts = 0;
        for_each_combo_indices(live, need, [&](const int* run, int run_k) {
            if ((++steps % kCancelStride) == 0) {
                throw_if_cancelled(cancel);
            }
            ++runouts;
            const std::uint64_t hero_later = strength_ids(h0, h1, board, board_n, run, run_k);
            const std::uint64_t vil_later =
                strength_ids(combo.card_a, combo.card_b, board, board_n, run, run_k);
            combo_hp[now][outcome_of(hero_later, vil_later)] += 1.0;
        });
        if (runouts <= 0) {
            continue;
        }
        const double scale = combo.weight / static_cast<double>(runouts);
        for (int a = 0; a < 3; ++a) {
            for (int b = 0; b < 3; ++b) {
                hp[a][b] += combo_hp[a][b] * scale;
            }
        }
    }
    apply_transitions(out, hp);
}

void accumulate_mc(int h0, int h1, const int* board, int board_n, int need, const SparseRange& range,
                   std::uint64_t hero_board_mask, std::size_t trials, std::uint32_t seed,
                   HandPotentialBreakdown& out, const CancelPredicate* cancel) {
    std::vector<const WeightedHoleCombo*> live_combos;
    live_combos.reserve(range.combos.size());
    double live_sum = 0.0;
    const std::uint64_t hero_now = strength_ids(h0, h1, board, board_n, nullptr, 0);
    for (const WeightedHoleCombo& combo : range.combos) {
        const std::uint64_t vil_mask =
            (std::uint64_t{1} << combo.card_a) | (std::uint64_t{1} << combo.card_b);
        if ((hero_board_mask & vil_mask) != 0) {
            continue;
        }
        live_combos.push_back(&combo);
        live_sum += combo.weight;
        const std::uint64_t vil_now =
            strength_ids(combo.card_a, combo.card_b, board, board_n, nullptr, 0);
        const int now = outcome_of(hero_now, vil_now);
        if (now == kAhead) {
            out.n_ahead += combo.weight;
        } else if (now == kTied) {
            out.n_tied += combo.weight;
        } else {
            out.n_behind += combo.weight;
        }
    }
    if (live_combos.empty() || live_sum <= 0.0 || trials == 0) {
        return;
    }

    double hp[3][3]{};
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> pick_combo(0.0, live_sum);
    for (std::size_t t = 0; t < trials; ++t) {
        if ((t % kCancelStride) == 0) {
            throw_if_cancelled(cancel);
        }
        const double pick = pick_combo(rng);
        double acc = 0.0;
        const WeightedHoleCombo* combo = live_combos.back();
        for (const WeightedHoleCombo* c : live_combos) {
            acc += c->weight;
            if (pick <= acc) {
                combo = c;
                break;
            }
        }
        const std::uint64_t vil_mask =
            (std::uint64_t{1} << combo->card_a) | (std::uint64_t{1} << combo->card_b);
        std::vector<int> live;
        live.reserve(52);
        const std::uint64_t dead = hero_board_mask | vil_mask;
        for (int i = 0; i < 52; ++i) {
            if ((dead & (std::uint64_t{1} << i)) == 0) {
                live.push_back(i);
            }
        }
        if (static_cast<int>(live.size()) < need) {
            continue;
        }
        int extra[2]{};
        if (need == 1) {
            std::uniform_int_distribution<int> d(0, static_cast<int>(live.size()) - 1);
            extra[0] = live[static_cast<std::size_t>(d(rng))];
        } else {
            std::uniform_int_distribution<int> d(0, static_cast<int>(live.size()) - 1);
            int i = d(rng);
            int j = d(rng);
            while (j == i) {
                j = d(rng);
            }
            extra[0] = live[static_cast<std::size_t>(i)];
            extra[1] = live[static_cast<std::size_t>(j)];
        }
        const std::uint64_t vil_now =
            strength_ids(combo->card_a, combo->card_b, board, board_n, nullptr, 0);
        const int now = outcome_of(hero_now, vil_now);
        const std::uint64_t hero_later = strength_ids(h0, h1, board, board_n, extra, need);
        const std::uint64_t vil_later =
            strength_ids(combo->card_a, combo->card_b, board, board_n, extra, need);
        add_transition(hp, 1.0, now, outcome_of(hero_later, vil_later));
    }
    const double trial_n = static_cast<double>(trials);
    const double mass = out.n_ahead + out.n_tied + out.n_behind;
    const double scale = mass > 0.0 ? mass / trial_n : 1.0 / trial_n;
    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            hp[a][b] *= scale;
        }
    }
    apply_transitions(out, hp);
}

HandPotentialBreakdown compute_for_ids(int h0, int h1, const int* board, int board_n, int need,
                                       const SparseRange& range, std::uint64_t hero_board_mask,
                                       std::size_t trials, std::uint32_t seed,
                                       const CancelPredicate* cancel) {
    HandPotentialBreakdown out{};
    if (trials > 0) {
        accumulate_mc(h0, h1, board, board_n, need, range, hero_board_mask, trials, seed, out, cancel);
    } else {
        accumulate_exact(h0, h1, board, board_n, need, range, hero_board_mask, out, cancel);
    }
    finish_breakdown(out);
    return out;
}

}  // namespace

HandPotentialBreakdown hand_potential_breakdown(const std::vector<Card>& hero_hole_cards,
                                                const std::vector<Card>& board_cards,
                                                const SparseRange& villain_range,
                                                PotentialStreets streets,
                                                const CancelPredicate* should_cancel) {
    validate_hero_board(hero_hole_cards, board_cards, streets);
    if (villain_range.combos.empty() || villain_range.weight_sum <= 0.0) {
        throw std::invalid_argument("villain range has no valid combos after blockers");
    }
    const int h0 = deck_index_from_card(hero_hole_cards[0]);
    const int h1 = deck_index_from_card(hero_hole_cards[1]);
    int board[5]{};
    const int board_n = static_cast<int>(board_cards.size());
    DeckBitset used;
    used.set(h0);
    used.set(h1);
    for (int i = 0; i < board_n; ++i) {
        board[i] = deck_index_from_card(board_cards[static_cast<std::size_t>(i)]);
        used.set(board[i]);
    }
    const int need = streets == PotentialStreets::Two ? 2 : 1;
    return compute_for_ids(h0, h1, board, board_n, need, villain_range, used.mask, 0, 0, should_cancel);
}

double hand_strength_vs_range(const std::vector<Card>& hero_hole_cards,
                              const std::vector<Card>& board_cards, const SparseRange& villain_range,
                              const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::One,
                                   should_cancel)
        .hs;
}

double positive_potential_vs_range(const std::vector<Card>& hero_hole_cards,
                                   const std::vector<Card>& board_cards,
                                   const SparseRange& villain_range,
                                   const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::One,
                                   should_cancel)
        .ppot;
}

double negative_potential_vs_range(const std::vector<Card>& hero_hole_cards,
                                   const std::vector<Card>& board_cards,
                                   const SparseRange& villain_range,
                                   const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::One,
                                   should_cancel)
        .npot;
}

double effective_hand_strength(const std::vector<Card>& hero_hole_cards,
                               const std::vector<Card>& board_cards, const SparseRange& villain_range,
                               const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::One,
                                   should_cancel)
        .ehs;
}

double effective_hand_strength_squared(const std::vector<Card>& hero_hole_cards,
                                       const std::vector<Card>& board_cards,
                                       const SparseRange& villain_range,
                                       const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::One,
                                   should_cancel)
        .ehs2;
}

double two_street_positive_potential(const std::vector<Card>& hero_hole_cards,
                                     const std::vector<Card>& board_cards,
                                     const SparseRange& villain_range,
                                     const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::Two,
                                   should_cancel)
        .ppot;
}

double two_street_negative_potential(const std::vector<Card>& hero_hole_cards,
                                     const std::vector<Card>& board_cards,
                                     const SparseRange& villain_range,
                                     const CancelPredicate* should_cancel) {
    return hand_potential_breakdown(hero_hole_cards, board_cards, villain_range, PotentialStreets::Two,
                                   should_cancel)
        .npot;
}

int equity_bucket_from_ehs(double ehs, int bucket_count) {
    if (bucket_count < 1) {
        throw std::invalid_argument("equityBucketFromEhs requires k >= 1");
    }
    if (!std::isfinite(ehs)) {
        throw std::invalid_argument("equityBucketFromEhs requires a finite EHS");
    }
    const double x = std::min(1.0, std::max(0.0, ehs));
    int bucket = static_cast<int>(std::floor(x * static_cast<double>(bucket_count)));
    if (bucket >= bucket_count) {
        bucket = bucket_count - 1;
    }
    if (bucket < 0) {
        bucket = 0;
    }
    return bucket;
}

std::vector<double> combo_ehs_table_vs_range(const std::vector<Card>& board_cards,
                                             const SparseRange& villain_range,
                                             const ComboEhsTableOptions& options,
                                             const CancelPredicate* should_cancel) {
    const std::size_t n = board_cards.size();
    if (n != 3 && n != 4) {
        throw std::invalid_argument("comboEhsTableVsRange requires a flop or turn (3 or 4 board cards)");
    }
    DeckBitset board_dead;
    board_dead.mark_cards(board_cards);
    int board[5]{};
    const int board_n = static_cast<int>(n);
    for (int i = 0; i < board_n; ++i) {
        board[i] = deck_index_from_card(board_cards[static_cast<std::size_t>(i)]);
    }

    std::vector<double> out(1326, 0.0);
    int idx = 0;
    std::size_t steps = 0;
    for (int a = 0; a < 52; ++a) {
        for (int b = a + 1; b < 52; ++b, ++idx) {
            if ((++steps % kCancelStride) == 0) {
                throw_if_cancelled(should_cancel);
            }
            if (board_dead.test(a) || board_dead.test(b)) {
                continue;
            }
            const std::uint64_t hero_board =
                board_dead.mask | (std::uint64_t{1} << a) | (std::uint64_t{1} << b);
            const HandPotentialBreakdown row =
                compute_for_ids(a, b, board, board_n, 1, villain_range, hero_board, options.trials,
                                options.seed, should_cancel);
            out[static_cast<std::size_t>(idx)] = row.ehs;
        }
    }
    return out;
}

}  // namespace poker
