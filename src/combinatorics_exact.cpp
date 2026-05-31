#include "poker/combinatorics_exact.hpp"

#include "poker/cancel.hpp"
#include "poker/card_string.hpp"
#include "poker/combo_enumerator.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/poker_math.hpp"
#include "poker/range_equity.hpp"
#include "poker/runout_enumerator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace poker {

namespace {

void fill_seven(int h0, int h1, const std::vector<Card>& board, const int* run, int run_k,
                std::uint8_t* ranks, std::uint8_t* suits) {
    ranks[0] = static_cast<std::uint8_t>(h0 / 4);
    suits[0] = static_cast<std::uint8_t>(h0 % 4);
    ranks[1] = static_cast<std::uint8_t>(h1 / 4);
    suits[1] = static_cast<std::uint8_t>(h1 % 4);
    const std::size_t bn = board.size();
    for (std::size_t bi = 0; bi < bn; ++bi) {
        ranks[2 + bi] = board[bi].rank();
        suits[2 + bi] = board[bi].suit();
    }
    const std::size_t base = 2 + bn;
    for (int ri = 0; ri < run_k; ++ri) {
        const int idx = run[ri];
        ranks[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx / 4);
        suits[base + static_cast<std::size_t>(ri)] = static_cast<std::uint8_t>(idx % 4);
    }
}

std::vector<Card> board_with_runout(const std::vector<Card>& board, const int* run, int run_k) {
    std::vector<Card> out = board;
    out.reserve(board.size() + static_cast<std::size_t>(run_k));
    for (int i = 0; i < run_k; ++i) {
        out.push_back(card_from_deck_index(run[i]));
    }
    return out;
}

int category_from_seven(const std::uint8_t ranks[7], const std::uint8_t suits[7]) {
    const HandEvaluation e = evaluate_best_hand_fast(ranks, suits, 7);
    return static_cast<int>(hand_category(e));
}

std::uint64_t strength_from_seven(const std::uint8_t ranks[7], const std::uint8_t suits[7]) {
    return evaluate_seven_strength_fast(ranks, suits);
}

std::uint64_t max_villain_strength_on_board(const std::vector<Card>& board, std::uint64_t dead_mask,
                                            int h0, int h1, const CancelPredicate* cancel) {
    std::uint64_t best = 0;
    std::vector<int> deck;
    deck.reserve(52);
    for (int i = 0; i < 52; ++i) {
        if ((dead_mask & (std::uint64_t{1} << i)) == 0 && i != h0 && i != h1) {
            deck.push_back(i);
        }
    }
    std::uint8_t vil_r[7]{};
    std::uint8_t vil_s[7]{};
    for_each_combo_indices(deck, 2, [&](const int* vc, int vc_k) {
        throw_if_cancelled(cancel);
        vil_r[0] = static_cast<std::uint8_t>(vc[0] / 4);
        vil_s[0] = static_cast<std::uint8_t>(vc[0] % 4);
        vil_r[1] = static_cast<std::uint8_t>(vc[1] / 4);
        vil_s[1] = static_cast<std::uint8_t>(vc[1] % 4);
        for (std::size_t bi = 0; bi < board.size(); ++bi) {
            vil_r[2 + bi] = board[bi].rank();
            vil_s[2 + bi] = board[bi].suit();
        }
        const std::uint64_t s = evaluate_seven_strength_fast(vil_r, vil_s);
        if (s > best) {
            best = s;
        }
    });
    return best;
}

bool any_villain_beats_hero(const std::vector<Card>& board, std::uint64_t dead_mask, int h0, int h1,
                            std::uint64_t hero_strength, const CancelPredicate* cancel) {
    std::vector<int> deck;
    for (int i = 0; i < 52; ++i) {
        if ((dead_mask & (std::uint64_t{1} << i)) == 0 && i != h0 && i != h1) {
            deck.push_back(i);
        }
    }
    std::uint8_t vil_r[7]{};
    std::uint8_t vil_s[7]{};
    bool found = false;
    for_each_combo_indices(deck, 2, [&](const int* vc, int) {
        if (found) {
            return;
        }
        throw_if_cancelled(cancel);
        vil_r[0] = static_cast<std::uint8_t>(vc[0] / 4);
        vil_s[0] = static_cast<std::uint8_t>(vc[0] % 4);
        vil_r[1] = static_cast<std::uint8_t>(vc[1] / 4);
        vil_s[1] = static_cast<std::uint8_t>(vc[1] % 4);
        for (std::size_t bi = 0; bi < board.size(); ++bi) {
            vil_r[2 + bi] = board[bi].rank();
            vil_s[2 + bi] = board[bi].suit();
        }
        if (evaluate_seven_strength_fast(vil_r, vil_s) > hero_strength) {
            found = true;
        }
    });
    return found;
}

void validate_board_runout(const std::vector<Card>& hero, const std::vector<Card>& board) {
    if (hero.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (board.size() < 3 || board.size() > 4) {
        throw std::invalid_argument("board must have 3 or 4 cards for runout enumeration");
    }
}

DeckBitset dead_from_cards(const std::vector<Card>& hero, const std::vector<Card>& board,
                           const std::vector<Card>& dead) {
    DeckBitset u;
    u.mark_cards(hero);
    u.mark_cards(board);
    u.mark_cards(dead);
    return u;
}

HeroEquityRunoutQuantilesResult quantiles_from_samples(std::vector<double>& samples) {
    HeroEquityRunoutQuantilesResult r;
    r.n = samples.size();
    if (samples.empty()) {
        return r;
    }
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    r.mean = sum / static_cast<double>(samples.size());
    double var_acc = 0.0;
    for (double x : samples) {
        const double d = x - r.mean;
        var_acc += d * d;
    }
    r.variance = var_acc / static_cast<double>(samples.size());
    std::sort(samples.begin(), samples.end());
    auto at_q = [&](double q) {
        const double pos = q * static_cast<double>(samples.size() - 1);
        const std::size_t lo = static_cast<std::size_t>(pos);
        const std::size_t hi = std::min(lo + 1, samples.size() - 1);
        const double t = pos - static_cast<double>(lo);
        return samples[lo] * (1.0 - t) + samples[hi] * t;
    };
    r.p05 = at_q(0.05);
    r.p50 = at_q(0.50);
    r.p95 = at_q(0.95);
    return r;
}

double equity_vs_random_on_fixed_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                                       std::uint64_t dead_mask, const CancelPredicate* cancel) {
    const int h0 = deck_index_from_card(hero[0]);
    const int h1 = deck_index_from_card(hero[1]);
    std::uint8_t hero_r[7]{};
    std::uint8_t hero_s[7]{};
    fill_seven(h0, h1, board, nullptr, 0, hero_r, hero_s);
    const std::uint64_t hero_s7 = evaluate_seven_strength_fast(hero_r, hero_s);
    double win = 0.0;
    double total = 0.0;
    std::vector<int> deck;
    for (int i = 0; i < 52; ++i) {
        if ((dead_mask & (std::uint64_t{1} << i)) == 0 && i != h0 && i != h1) {
            deck.push_back(i);
        }
    }
    std::uint8_t vil_r[7]{};
    std::uint8_t vil_s[7]{};
    for_each_combo_indices(deck, 2, [&](const int* vc, int) {
        throw_if_cancelled(cancel);
        vil_r[0] = static_cast<std::uint8_t>(vc[0] / 4);
        vil_s[0] = static_cast<std::uint8_t>(vc[0] % 4);
        vil_r[1] = static_cast<std::uint8_t>(vc[1] / 4);
        vil_s[1] = static_cast<std::uint8_t>(vc[1] % 4);
        for (std::size_t bi = 0; bi < board.size(); ++bi) {
            vil_r[2 + bi] = board[bi].rank();
            vil_s[2 + bi] = board[bi].suit();
        }
        total += 1.0;
        const std::uint64_t vs = evaluate_seven_strength_fast(vil_r, vil_s);
        if (hero_s7 > vs) {
            win += 1.0;
        } else if (hero_s7 == vs) {
            win += 0.5;
        }
    });
    return total > 0.0 ? win / total : 0.0;
}

}  // namespace

HeroRunoutVulnerabilityResult exact_hero_runout_vulnerability(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel) {
    validate_board_runout(hero_hole_cards, board_cards);
    const DeckBitset used = dead_from_cards(hero_hole_cards, board_cards, known_dead_cards);
    const int h0 = deck_index_from_card(hero_hole_cards[0]);
    const int h1 = deck_index_from_card(hero_hole_cards[1]);
    const RunoutEnumerator en =
        make_runout_enumerator_from_dead_mask(used.mask, static_cast<int>(board_cards.size()));
    HeroRunoutVulnerabilityResult out;
    std::uint64_t nuts_count = 0;
    std::uint64_t dom_count = 0;
    for_each_uniform_runout(en, [&](const int* run, int run_k) {
        throw_if_cancelled(cancel);
        const std::vector<Card> full_board = board_with_runout(board_cards, run, run_k);
        std::uint64_t dead = used.mask;
        for (int i = 0; i < run_k; ++i) {
            dead |= std::uint64_t{1} << run[i];
        }
        std::uint8_t hero_r[7]{};
        std::uint8_t hero_s[7]{};
        fill_seven(h0, h1, full_board, nullptr, 0, hero_r, hero_s);
        const std::uint64_t hero_st = evaluate_seven_strength_fast(hero_r, hero_s);
        const std::uint64_t max_vil = max_villain_strength_on_board(full_board, dead, h0, h1, cancel);
        if (hero_st >= max_vil) {
            ++nuts_count;
        }
        if (any_villain_beats_hero(full_board, dead, h0, h1, hero_st, cancel)) {
            ++dom_count;
        }
        ++out.runout_count;
    }, cancel);
    if (out.runout_count > 0) {
        const double n = static_cast<double>(out.runout_count);
        out.p_nuts = static_cast<double>(nuts_count) / n;
        out.p_dominated = static_cast<double>(dom_count) / n;
    }
    return out;
}

VillainLeapfrogResult exact_villain_leapfrog_out_counts(const std::vector<Card>& hero_hole_cards,
                                                        const std::vector<Card>& board_cards,
                                                        const std::vector<Card>& known_dead_cards,
                                                        const CancelPredicate* cancel) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (board_cards.size() < 3 || board_cards.size() > 4) {
        throw std::invalid_argument("board must have 3 or 4 cards");
    }
    const DeckBitset used = dead_from_cards(hero_hole_cards, board_cards, known_dead_cards);
    const int h0 = deck_index_from_card(hero_hole_cards[0]);
    const int h1 = deck_index_from_card(hero_hole_cards[1]);
    std::uint8_t hero_r[7]{};
    std::uint8_t hero_s[7]{};
    fill_seven(h0, h1, board_cards, nullptr, 0, hero_r, hero_s);
    const std::uint64_t hero_base = evaluate_seven_strength_fast(hero_r, hero_s);
    const std::uint64_t vil_base_max =
        max_villain_strength_on_board(board_cards, used.mask, h0, h1, cancel);

    VillainLeapfrogResult out;
    std::vector<int> remaining;
    remaining.reserve(52);
    for (int i = 0; i < 52; ++i) {
        if (!used.test(i)) {
            remaining.push_back(i);
        }
    }
    for (int c : remaining) {
        throw_if_cancelled(cancel);
        std::vector<Card> next_board = board_cards;
        next_board.push_back(card_from_deck_index(c));
        std::uint64_t dead = used.mask | (std::uint64_t{1} << c);
        fill_seven(h0, h1, next_board, nullptr, 0, hero_r, hero_s);
        const std::uint64_t hero_st = evaluate_seven_strength_fast(hero_r, hero_s);
        const std::uint64_t vil_max = max_villain_strength_on_board(next_board, dead, h0, h1, cancel);
        if (vil_max > hero_st && vil_max > vil_base_max) {
            out.leapfrog_deck_indices.push_back(c);
        }
        if (hero_st > hero_base) {
            out.hero_improve_deck_indices.push_back(c);
        }
    }
    return out;
}

std::vector<double> exact_hero_category_joint_flop_to_river(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& flop_three,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (flop_three.size() != 3) {
        throw std::invalid_argument("flop must have exactly three cards");
    }
    const DeckBitset used = dead_from_cards(hero_hole_cards, flop_three, known_dead_cards);
    const int h0 = deck_index_from_card(hero_hole_cards[0]);
    const int h1 = deck_index_from_card(hero_hole_cards[1]);
    std::array<double, 81> joint{};
    const RunoutEnumerator en = make_runout_enumerator_from_dead_mask(used.mask, 3);
    double total = 0.0;
    for_each_uniform_runout(en, [&](const int* run, int run_k) {
        throw_if_cancelled(cancel);
        if (run_k != 2) {
            return;
        }
        std::vector<Card> turn_board = flop_three;
        turn_board.push_back(card_from_deck_index(run[0]));
        std::vector<Card> river_board = turn_board;
        river_board.push_back(card_from_deck_index(run[1]));
        std::uint8_t hero_r[7]{};
        std::uint8_t hero_s[7]{};
        fill_seven(h0, h1, turn_board, nullptr, 0, hero_r, hero_s);
        const int cat_turn = category_from_seven(hero_r, hero_s);
        fill_seven(h0, h1, river_board, nullptr, 0, hero_r, hero_s);
        const int cat_river = category_from_seven(hero_r, hero_s);
        joint[static_cast<std::size_t>(cat_turn * 9 + cat_river)] += 1.0;
        total += 1.0;
    }, cancel);
    if (total > 0.0) {
        for (double& v : joint) {
            v /= total;
        }
    }
    return std::vector<double>(joint.begin(), joint.end());
}

double exact_range_dominated_combo_fraction(const std::vector<Card>& hero_hole_cards,
                                            const std::vector<Card>& board_cards,
                                            const SparseRange& villain_range,
                                            const CancelPredicate* cancel) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (board_cards.size() != 5) {
        throw std::invalid_argument("exactRangeDominatedComboFraction: board must have 5 cards");
    }
    if (villain_range.combos.empty() || villain_range.weight_sum <= 0.0) {
        throw std::invalid_argument("villain range must have positive weight");
    }
    DeckBitset used;
    used.mark_cards(hero_hole_cards);
    used.mark_cards(board_cards);
    const int h0 = deck_index_from_card(hero_hole_cards[0]);
    const int h1 = deck_index_from_card(hero_hole_cards[1]);
    std::uint8_t hero_r[7]{};
    std::uint8_t hero_s[7]{};
    fill_seven(h0, h1, board_cards, nullptr, 0, hero_r, hero_s);
    const std::uint64_t hero_st = evaluate_seven_strength_fast(hero_r, hero_s);
    double dominated = 0.0;
    double total = 0.0;
    std::uint8_t vil_r[7]{};
    std::uint8_t vil_s[7]{};
    for (const WeightedHoleCombo& c : villain_range.combos) {
        throw_if_cancelled(cancel);
        if (used.test(c.card_a) || used.test(c.card_b)) {
            continue;
        }
        vil_r[0] = static_cast<std::uint8_t>(c.card_a / 4);
        vil_s[0] = static_cast<std::uint8_t>(c.card_a % 4);
        vil_r[1] = static_cast<std::uint8_t>(c.card_b / 4);
        vil_s[1] = static_cast<std::uint8_t>(c.card_b % 4);
        for (std::size_t bi = 0; bi < board_cards.size(); ++bi) {
            vil_r[2 + bi] = board_cards[bi].rank();
            vil_s[2 + bi] = board_cards[bi].suit();
        }
        total += c.weight;
        if (evaluate_seven_strength_fast(vil_r, vil_s) < hero_st) {
            dominated += c.weight;
        }
    }
    if (total <= 0.0) {
        throw std::invalid_argument("no live combos in range");
    }
    return dominated / total;
}

HeroEquityRunoutQuantilesResult exact_hero_equity_runout_quantiles_vs_random(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const std::vector<Card>& known_dead_cards, const CancelPredicate* cancel) {
    if (hero_hole_cards.size() != 2) {
        throw std::invalid_argument("hero must have exactly two cards");
    }
    if (board_cards.size() > 3) {
        throw std::invalid_argument("board must have at most 3 cards for runout quantiles");
    }
    const DeckBitset used = dead_from_cards(hero_hole_cards, board_cards, known_dead_cards);
    const RunoutEnumerator en =
        make_runout_enumerator_from_dead_mask(used.mask, static_cast<int>(board_cards.size()));
    std::vector<double> samples;
    for_each_uniform_runout(en, [&](const int* run, int run_k) {
        throw_if_cancelled(cancel);
        const std::vector<Card> full = board_with_runout(board_cards, run, run_k);
        std::uint64_t dead = used.mask;
        for (int i = 0; i < run_k; ++i) {
            dead |= std::uint64_t{1} << run[i];
        }
        samples.push_back(equity_vs_random_on_fixed_board(hero_hole_cards, full, dead, cancel));
    }, cancel);
    return quantiles_from_samples(samples);
}

HeroEquityRunoutQuantilesResult exact_hero_equity_runout_quantiles_vs_range(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, const CancelPredicate* cancel) {
    if (board_cards.size() > 3) {
        throw std::invalid_argument("board must have at most 3 cards for runout quantiles vs range");
    }
    const DeckBitset used = dead_from_cards(hero_hole_cards, board_cards, {});
    const RunoutEnumerator en =
        make_runout_enumerator_from_dead_mask(used.mask, static_cast<int>(board_cards.size()));
    std::vector<double> samples;
    for_each_uniform_runout(en, [&](const int* run, int run_k) {
        throw_if_cancelled(cancel);
        const std::vector<Card> full = board_with_runout(board_cards, run, run_k);
        samples.push_back(exact_hu_equity_vs_range(hero_hole_cards, full, villain_range, cancel));
    }, cancel);
    return quantiles_from_samples(samples);
}

CardRemovalGradientResult exact_equity_card_removal_gradient(
    const std::vector<Card>& hero_hole_cards, const std::vector<Card>& board_cards,
    const SparseRange& villain_range, const CancelPredicate* cancel) {
    CardRemovalGradientResult out;
    out.base_equity =
        exact_hu_equity_vs_range(hero_hole_cards, board_cards, villain_range, cancel);
    DeckBitset base_dead;
    base_dead.mark_cards(hero_hole_cards);
    base_dead.mark_cards(board_cards);
    for (int c = 0; c < 52; ++c) {
        throw_if_cancelled(cancel);
        if (base_dead.test(c)) {
            out.gradient[static_cast<std::size_t>(c)] = 0.0;
            continue;
        }
        SparseRange filtered;
        filtered.weight_sum = 0.0;
        const std::uint64_t dead = base_dead.mask | (std::uint64_t{1} << c);
        for (const WeightedHoleCombo& combo : villain_range.combos) {
            if ((dead & (std::uint64_t{1} << combo.card_a)) != 0 ||
                (dead & (std::uint64_t{1} << combo.card_b)) != 0) {
                continue;
            }
            filtered.combos.push_back(combo);
            filtered.weight_sum += combo.weight;
        }
        if (filtered.combos.empty()) {
            out.gradient[static_cast<std::size_t>(c)] = 0.0;
            continue;
        }
        const double eq =
            exact_hu_equity_vs_range(hero_hole_cards, board_cards, filtered, cancel);
        out.gradient[static_cast<std::size_t>(c)] = out.base_equity - eq;
    }
    return out;
}

double exact_information_regret_vs_clairvoyant(const std::vector<Card>& hero_hole_cards,
                                               const std::vector<Card>& board_cards,
                                               const SparseRange& villain_range,
                                               double pot_before_call, double to_call,
                                               const CancelPredicate* cancel) {
    if (to_call < 0.0 || pot_before_call < 0.0) {
        throw std::invalid_argument("pot and toCall must be non-negative");
    }
    const double realistic =
        exact_hu_equity_vs_range(hero_hole_cards, board_cards, villain_range, cancel);
    const double ev_realistic =
        expected_value_call(realistic, static_cast<int>(pot_before_call),
                            static_cast<int>(to_call));

    if (board_cards.size() == 5) {
        const double clair = realistic;
        const double ev_clair =
            expected_value_call(clair, static_cast<int>(pot_before_call), static_cast<int>(to_call));
        return ev_clair - ev_realistic;
    }

    if (board_cards.size() < 3 || board_cards.size() > 4) {
        throw std::invalid_argument("board must have 3, 4, or 5 cards");
    }

    const DeckBitset used = dead_from_cards(hero_hole_cards, board_cards, {});
    const RunoutEnumerator en =
        make_runout_enumerator_from_dead_mask(used.mask, static_cast<int>(board_cards.size()));
    double sum_clair_ev = 0.0;
    double count = 0.0;
    for_each_uniform_runout(en, [&](const int* run, int run_k) {
        throw_if_cancelled(cancel);
        const std::vector<Card> full = board_with_runout(board_cards, run, run_k);
        const double eq = exact_hu_equity_vs_range(hero_hole_cards, full, villain_range, cancel);
        const double ev_fold = 0.0;
        const double ev_call =
            expected_value_call(eq, static_cast<int>(pot_before_call), static_cast<int>(to_call));
        sum_clair_ev += std::max(ev_fold, ev_call);
        count += 1.0;
    }, cancel);
    const double ev_clairvoyant = count > 0.0 ? sum_clair_ev / count : 0.0;
    return ev_clairvoyant - ev_realistic;
}

}  // namespace poker
