#include "poker/cfr_subgame.hpp"

#include "poker/card_string.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/monte_carlo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>

namespace poker {

namespace {

constexpr int kComboCount = 1326;
constexpr int kDefaultIters = 400;
constexpr int kMaxIters = 20000;
constexpr int kPreflopMcTrials = 600;

[[nodiscard]] int combo_index_1326(int a, int b) {
    if (a > b) {
        std::swap(a, b);
    }
    if (a < 0 || b > 51 || a == b) {
        return -1;
    }
    return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

[[nodiscard]] double clamp01(double x) {
    if (!std::isfinite(x)) {
        return 0.0;
    }
    return std::min(1.0, std::max(0.0, x));
}

[[nodiscard]] int clamp_iterations(int iterations) {
    if (iterations <= 0) {
        return kDefaultIters;
    }
    return std::min(kMaxIters, iterations);
}

void require_finite_positive(double x, const char* name) {
    if (!std::isfinite(x) || x <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and > 0");
    }
}

void require_river_board(const std::vector<Card>& board) {
    if (board.size() != 5) {
        throw std::invalid_argument("river board must have 5 cards");
    }
    if (cards_have_duplicate(board)) {
        throw std::invalid_argument("duplicate cards on board");
    }
}

struct LiveCombo {
    int card_a{0};
    int card_b{0};
    int index{0};
    double weight{0.0};
    std::uint64_t strength{0};
    std::uint64_t mask{0};
    HandRank category{HandRank::HighCard};
};

[[nodiscard]] std::vector<LiveCombo> live_combos(const SparseRange& range, const std::vector<Card>& board,
                                                 bool eval_strength) {
    DeckBitset dead;
    dead.mark_cards(board);
    std::vector<LiveCombo> out;
    out.reserve(range.combos.size());
    for (const WeightedHoleCombo& c : range.combos) {
        if (c.weight <= 0.0 || !std::isfinite(c.weight)) {
            continue;
        }
        if (dead.test(c.card_a) || dead.test(c.card_b) || c.card_a == c.card_b) {
            continue;
        }
        LiveCombo live{};
        live.card_a = c.card_a;
        live.card_b = c.card_b;
        live.index = combo_index_1326(c.card_a, c.card_b);
        live.weight = c.weight;
        live.mask = (std::uint64_t{1} << c.card_a) | (std::uint64_t{1} << c.card_b);
        if (eval_strength) {
            const std::vector<Card> hole{card_from_deck_index(c.card_a), card_from_deck_index(c.card_b)};
            live.strength = evaluate_hand_strength_fast(hole, board);
            live.category = evaluate_hand(hole, board);
        }
        out.push_back(live);
    }
    if (out.empty()) {
        throw std::invalid_argument("range has no live combos after board blockers");
    }
    return out;
}

[[nodiscard]] bool overlap(const LiveCombo& a, const LiveCombo& b) { return (a.mask & b.mask) != 0; }

[[nodiscard]] double river_equity(const LiveCombo& bettor, const LiveCombo& defender) {
    if (bettor.strength > defender.strength) {
        return 1.0;
    }
    if (bettor.strength < defender.strength) {
        return 0.0;
    }
    return 0.5;
}

[[nodiscard]] double mix_at(const std::vector<double>& mix, const LiveCombo& combo) {
    if (mix.size() == 1) {
        return clamp01(mix[0]);
    }
    if (mix.size() == static_cast<std::size_t>(kComboCount) && combo.index >= 0) {
        return clamp01(mix[static_cast<std::size_t>(combo.index)]);
    }
    throw std::invalid_argument("mix must be a scalar or length-1326 weights");
}

[[nodiscard]] std::vector<double> empty_mix_1326() { return std::vector<double>(static_cast<std::size_t>(kComboCount), 0.0); }

void write_mix_1326(std::vector<double>& dest, const std::vector<LiveCombo>& combos,
                    const std::vector<double>& local) {
    dest.assign(static_cast<std::size_t>(kComboCount), 0.0);
    for (std::size_t i = 0; i < combos.size(); ++i) {
        if (combos[i].index >= 0) {
            dest[static_cast<std::size_t>(combos[i].index)] = clamp01(local[i]);
        }
    }
}

[[nodiscard]] double bettor_leaf(double eq, double pot, double bet, double call_p) {
    const double fold_p = 1.0 - call_p;
    const double ev_call = eq * (pot + 2.0 * bet) - bet;
    return fold_p * pot + call_p * ev_call;
}

struct PairAcc {
    double z{0.0};
    double ev_check{0.0};
    double ev_bet{0.0};
};

void add_pair_check_bet(PairAcc& acc, double w, double eq, double pot, double bet, double call_p) {
    acc.z += w;
    acc.ev_check += w * (eq * pot);
    acc.ev_bet += w * bettor_leaf(eq, pot, bet, call_p);
}

[[nodiscard]] double defender_call_ev(double eq_bettor, double pot, double bet) {
    return (1.0 - eq_bettor) * (pot + 2.0 * bet) - bet;
}

void require_mix(const std::vector<double>& mix) {
    if (mix.size() != 1 && mix.size() != static_cast<std::size_t>(kComboCount)) {
        throw std::invalid_argument("mix must be a scalar or length-1326 weights");
    }
}

struct RiverWorkspace {
    std::vector<LiveCombo> bettor;
    std::vector<LiveCombo> defender;
    std::vector<double> eq;  // row-major bettor * defender; -1 if overlap
    double pot{0.0};
    double bet{0.0};

    [[nodiscard]] double equity(std::size_t i, std::size_t j) const { return eq[i * defender.size() + j]; }
};

[[nodiscard]] RiverWorkspace make_river_workspace(double pot, double bet_size, const SparseRange& bettor_range,
                                                  const SparseRange& defender_range,
                                                  const std::vector<Card>& board) {
    require_finite_positive(pot, "pot");
    require_finite_positive(bet_size, "betSize");
    require_river_board(board);
    RiverWorkspace ws;
    ws.pot = pot;
    ws.bet = bet_size;
    ws.bettor = live_combos(bettor_range, board, true);
    ws.defender = live_combos(defender_range, board, true);
    ws.eq.assign(ws.bettor.size() * ws.defender.size(), -1.0);
    for (std::size_t i = 0; i < ws.bettor.size(); ++i) {
        for (std::size_t j = 0; j < ws.defender.size(); ++j) {
            if (!overlap(ws.bettor[i], ws.defender[j])) {
                ws.eq[i * ws.defender.size() + j] = river_equity(ws.bettor[i], ws.defender[j]);
            }
        }
    }
    return ws;
}

void values_for_bettor(const RiverWorkspace& ws, std::size_t i, const std::vector<double>& call_p, double& v_check,
                       double& v_bet) {
    PairAcc acc;
    for (std::size_t j = 0; j < ws.defender.size(); ++j) {
        const double e = ws.equity(i, j);
        if (e < 0.0) {
            continue;
        }
        add_pair_check_bet(acc, ws.defender[j].weight, e, ws.pot, ws.bet, call_p[j]);
    }
    if (acc.z <= 0.0) {
        v_check = 0.0;
        v_bet = 0.0;
        return;
    }
    v_check = acc.ev_check / acc.z;
    v_bet = acc.ev_bet / acc.z;
}

void values_for_defender(const RiverWorkspace& ws, std::size_t j, const std::vector<double>& bet_p, double& v_fold,
                         double& v_call) {
    v_fold = 0.0;
    double z = 0.0;
    double ev_call = 0.0;
    for (std::size_t i = 0; i < ws.bettor.size(); ++i) {
        const double e = ws.equity(i, j);
        if (e < 0.0) {
            continue;
        }
        const double w = ws.bettor[i].weight * bet_p[i];
        if (w <= 0.0) {
            continue;
        }
        z += w;
        ev_call += w * defender_call_ev(e, ws.pot, ws.bet);
    }
    v_call = z > 0.0 ? ev_call / z : 0.0;
}

[[nodiscard]] RiverProfileEv profile_ev(const RiverWorkspace& ws, const std::vector<double>& bet_p,
                                        const std::vector<double>& call_p) {
    double z = 0.0;
    double ev_b = 0.0;
    for (std::size_t i = 0; i < ws.bettor.size(); ++i) {
        for (std::size_t j = 0; j < ws.defender.size(); ++j) {
            const double e = ws.equity(i, j);
            if (e < 0.0) {
                continue;
            }
            const double w = ws.bettor[i].weight * ws.defender[j].weight;
            z += w;
            const double check_ev = e * ws.pot;
            const double bet_ev = bettor_leaf(e, ws.pot, ws.bet, call_p[j]);
            ev_b += w * ((1.0 - bet_p[i]) * check_ev + bet_p[i] * bet_ev);
        }
    }
    RiverProfileEv out;
    if (z <= 0.0) {
        return out;
    }
    out.ev_bettor = ev_b / z;
    out.ev_defender = ws.pot - out.ev_bettor;
    return out;
}

[[nodiscard]] double range_weighted_freq(const std::vector<LiveCombo>& combos, const std::vector<double>& p) {
    double z = 0.0;
    double s = 0.0;
    for (std::size_t i = 0; i < combos.size(); ++i) {
        z += combos[i].weight;
        s += combos[i].weight * p[i];
    }
    return z > 0.0 ? s / z : 0.0;
}

void br_bettor_mix(const RiverWorkspace& ws, const std::vector<double>& call_p, std::vector<double>& bet_p,
                   double& value) {
    bet_p.assign(ws.bettor.size(), 0.0);
    double z = 0.0;
    double ev = 0.0;
    for (std::size_t i = 0; i < ws.bettor.size(); ++i) {
        double v_check = 0.0;
        double v_bet = 0.0;
        values_for_bettor(ws, i, call_p, v_check, v_bet);
        bet_p[i] = v_bet >= v_check ? 1.0 : 0.0;
        double row = 0.0;
        for (std::size_t j = 0; j < ws.defender.size(); ++j) {
            if (ws.equity(i, j) >= 0.0) {
                row += ws.defender[j].weight;
            }
        }
        const double w = ws.bettor[i].weight * row;
        z += w;
        ev += w * std::max(v_check, v_bet);
    }
    value = z > 0.0 ? ev / z : 0.0;
}

void br_defender_mix(const RiverWorkspace& ws, const std::vector<double>& bet_p, std::vector<double>& call_p,
                     double& value) {
    call_p.assign(ws.defender.size(), 0.0);
    double z = 0.0;
    double ev = 0.0;
    for (std::size_t j = 0; j < ws.defender.size(); ++j) {
        double v_fold = 0.0;
        double v_call = 0.0;
        values_for_defender(ws, j, bet_p, v_fold, v_call);
        call_p[j] = v_call >= v_fold ? 1.0 : 0.0;
        // Defender EV includes check-back mass, not only the bet node.
        double pair_z = 0.0;
        double pair_ev = 0.0;
        for (std::size_t i = 0; i < ws.bettor.size(); ++i) {
            const double e = ws.equity(i, j);
            if (e < 0.0) {
                continue;
            }
            const double w = ws.bettor[i].weight * ws.defender[j].weight;
            pair_z += w;
            const double check_ev = (1.0 - e) * ws.pot;
            const double mixed = (1.0 - bet_p[i]) * check_ev +
                                 bet_p[i] * (call_p[j] > 0.5 ? defender_call_ev(e, ws.pot, ws.bet) : 0.0);
            pair_ev += w * mixed;
        }
        z += pair_z;
        ev += pair_ev;
    }
    value = z > 0.0 ? ev / z : 0.0;
}

void fill_local_mix(const std::vector<double>& mix, const std::vector<LiveCombo>& combos,
                    std::vector<double>& local) {
    require_mix(mix);
    local.resize(combos.size());
    for (std::size_t i = 0; i < combos.size(); ++i) {
        local[i] = mix_at(mix, combos[i]);
    }
}

[[nodiscard]] RiverCfrResult pack_river_result(const RiverWorkspace& ws, const std::vector<double>& bet_p,
                                               const std::vector<double>& call_p, int iterations) {
    RiverCfrResult out;
    out.iterations = iterations;
    out.bet_freq = range_weighted_freq(ws.bettor, bet_p);
    out.call_freq = range_weighted_freq(ws.defender, call_p);
    const RiverProfileEv ev = profile_ev(ws, bet_p, call_p);
    out.ev_bettor = ev.ev_bettor;
    out.ev_defender = ev.ev_defender;
    write_mix_1326(out.bet_mix_1326, ws.bettor, bet_p);
    write_mix_1326(out.call_mix_1326, ws.defender, call_p);
    return out;
}

[[nodiscard]] int class_bucket(HandRank rank) {
    // river: no unfinished draws — pair-or-worse high-card is air; one pair is the "draw" stand-in
    // (weak made); two pair is made; trips+ is strong.
    switch (rank) {
        case HandRank::HighCard:
            return 0;
        case HandRank::OnePair:
            return 1;
        case HandRank::TwoPair:
            return 2;
        default:
            return 3;
    }
}

}  // namespace

std::vector<double> regret_matching_strategy(const std::vector<double>& regrets) {
    if (regrets.empty()) {
        throw std::invalid_argument("regrets must be non-empty");
    }
    std::vector<double> pos(regrets.size(), 0.0);
    double sum = 0.0;
    for (std::size_t i = 0; i < regrets.size(); ++i) {
        const double x = std::isfinite(regrets[i]) ? std::max(0.0, regrets[i]) : 0.0;
        pos[i] = x;
        sum += x;
    }
    if (sum <= 0.0) {
        const double u = 1.0 / static_cast<double>(regrets.size());
        return std::vector<double>(regrets.size(), u);
    }
    for (double& x : pos) {
        x /= sum;
    }
    return pos;
}

CfrNodeUpdate cfr_node_reach_update(const std::vector<double>& cumulative_regrets,
                                    const std::vector<double>& instantaneous_regrets, double reach) {
    if (cumulative_regrets.size() != instantaneous_regrets.size() || cumulative_regrets.empty()) {
        throw std::invalid_argument("cfrNodeReachUpdate needs matching non-empty regret vectors");
    }
    if (!std::isfinite(reach) || reach < 0.0) {
        throw std::invalid_argument("reach must be finite and ≥ 0");
    }
    CfrNodeUpdate out;
    out.regrets.resize(cumulative_regrets.size());
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        const double inst = std::isfinite(instantaneous_regrets[i]) ? instantaneous_regrets[i] : 0.0;
        const double prev = std::isfinite(cumulative_regrets[i]) ? cumulative_regrets[i] : 0.0;
        out.regrets[i] = prev + reach * inst;
    }
    out.strategy = regret_matching_strategy(out.regrets);
    return out;
}

StrategySupportSize strategy_support_size(const std::vector<double>& action_probs, double eps) {
    if (!std::isfinite(eps) || eps < 0.0 || eps >= 0.5) {
        throw std::invalid_argument("eps must be in [0, 0.5)");
    }
    StrategySupportSize out;
    if (action_probs.empty()) {
        return out;
    }
    double pure = 0.0;
    int pure_n = 0;
    for (double p : action_probs) {
        if (!std::isfinite(p)) {
            continue;
        }
        const double x = clamp01(p);
        if (x > eps && x < 1.0 - eps) {
            ++out.mixed_count;
        }
        if (x >= 1.0 - eps) {
            pure += x;
            ++pure_n;
        }
    }
    out.pure_mass = pure_n > 0 ? pure / static_cast<double>(action_probs.size()) : 0.0;
    return out;
}

RiverCfrResult cfr_river_bet_call_fold_solve(double pot, double bet_size, const SparseRange& bettor_range,
                                             const SparseRange& defender_range, const std::vector<Card>& board,
                                             int iterations) {
    const int T = clamp_iterations(iterations);
    const RiverWorkspace ws = make_river_workspace(pot, bet_size, bettor_range, defender_range, board);
    const std::size_t nB = ws.bettor.size();
    const std::size_t nD = ws.defender.size();
    std::vector<std::vector<double>> r_bet(nB, std::vector<double>(2, 0.0));
    std::vector<std::vector<double>> r_call(nD, std::vector<double>(2, 0.0));
    std::vector<double> sum_bet(nB, 0.0);
    std::vector<double> sum_call(nD, 0.0);
    std::vector<double> bet_p(nB, 0.5);
    std::vector<double> call_p(nD, 0.5);

    for (int t = 0; t < T; ++t) {
        for (std::size_t i = 0; i < nB; ++i) {
            bet_p[i] = regret_matching_strategy(r_bet[i])[1];
        }
        for (std::size_t j = 0; j < nD; ++j) {
            call_p[j] = regret_matching_strategy(r_call[j])[1];
        }
        for (std::size_t i = 0; i < nB; ++i) {
            sum_bet[i] += bet_p[i];
        }
        for (std::size_t j = 0; j < nD; ++j) {
            sum_call[j] += call_p[j];
        }
        for (std::size_t i = 0; i < nB; ++i) {
            double v_check = 0.0;
            double v_bet = 0.0;
            values_for_bettor(ws, i, call_p, v_check, v_bet);
            const double v_sigma = (1.0 - bet_p[i]) * v_check + bet_p[i] * v_bet;
            r_bet[i][0] += v_check - v_sigma;
            r_bet[i][1] += v_bet - v_sigma;
        }
        for (std::size_t j = 0; j < nD; ++j) {
            double v_fold = 0.0;
            double v_call = 0.0;
            values_for_defender(ws, j, bet_p, v_fold, v_call);
            const double v_sigma = (1.0 - call_p[j]) * v_fold + call_p[j] * v_call;
            r_call[j][0] += v_fold - v_sigma;
            r_call[j][1] += v_call - v_sigma;
        }
    }
    for (std::size_t i = 0; i < nB; ++i) {
        bet_p[i] = sum_bet[i] / static_cast<double>(T);
    }
    for (std::size_t j = 0; j < nD; ++j) {
        call_p[j] = sum_call[j] / static_cast<double>(T);
    }
    return pack_river_result(ws, bet_p, call_p, T);
}

RiverCfrResult fictitious_play_river(double pot, double bet_size, const SparseRange& bettor_range,
                                     const SparseRange& defender_range, const std::vector<Card>& board,
                                     int iterations) {
    const int T = clamp_iterations(iterations);
    const RiverWorkspace ws = make_river_workspace(pot, bet_size, bettor_range, defender_range, board);
    std::vector<double> avg_bet(ws.bettor.size(), 0.5);
    std::vector<double> avg_call(ws.defender.size(), 0.5);
    std::vector<double> br_bet;
    std::vector<double> br_call;
    double unused = 0.0;
    for (int t = 0; t < T; ++t) {
        br_bettor_mix(ws, avg_call, br_bet, unused);
        br_defender_mix(ws, avg_bet, br_call, unused);
        const double n = static_cast<double>(t + 1);
        for (std::size_t i = 0; i < avg_bet.size(); ++i) {
            avg_bet[i] = (avg_bet[i] * static_cast<double>(t) + br_bet[i]) / n;
        }
        for (std::size_t j = 0; j < avg_call.size(); ++j) {
            avg_call[j] = (avg_call[j] * static_cast<double>(t) + br_call[j]) / n;
        }
    }
    return pack_river_result(ws, avg_bet, avg_call, T);
}

RiverProfileEv ev_of_strategy_profile(double pot, double bet_size, const SparseRange& bettor_range,
                                      const SparseRange& defender_range, const std::vector<Card>& board,
                                      const std::vector<double>& bettor_mix,
                                      const std::vector<double>& caller_mix) {
    const RiverWorkspace ws = make_river_workspace(pot, bet_size, bettor_range, defender_range, board);
    std::vector<double> bet_p;
    std::vector<double> call_p;
    fill_local_mix(bettor_mix, ws.bettor, bet_p);
    fill_local_mix(caller_mix, ws.defender, call_p);
    return profile_ev(ws, bet_p, call_p);
}

BestResponseRiverResult best_response_river(double pot, double bet_size, const SparseRange& hero_range,
                                            const SparseRange& villain_range, const std::vector<Card>& board,
                                            const std::vector<double>& villain_bet_mix) {
    const RiverWorkspace ws = make_river_workspace(pot, bet_size, villain_range, hero_range, board);
    std::vector<double> bet_p;
    fill_local_mix(villain_bet_mix, ws.bettor, bet_p);
    std::vector<double> call_p;
    double value = 0.0;
    br_defender_mix(ws, bet_p, call_p, value);
    BestResponseRiverResult out;
    out.value = value;
    out.call_frequency = range_weighted_freq(ws.defender, call_p);
    out.action = out.call_frequency >= 0.5 ? "call" : "fold";
    return out;
}

double exploitability_river(double pot, double bet_size, const SparseRange& bettor_range,
                            const SparseRange& defender_range, const std::vector<Card>& board,
                            const std::vector<double>& bettor_mix, const std::vector<double>& caller_mix) {
    const RiverWorkspace ws = make_river_workspace(pot, bet_size, bettor_range, defender_range, board);
    std::vector<double> bet_p;
    std::vector<double> call_p;
    fill_local_mix(bettor_mix, ws.bettor, bet_p);
    fill_local_mix(caller_mix, ws.defender, call_p);
    const RiverProfileEv ev = profile_ev(ws, bet_p, call_p);
    std::vector<double> br_bet;
    std::vector<double> br_call;
    double br0 = 0.0;
    double br1 = 0.0;
    br_bettor_mix(ws, call_p, br_bet, br0);
    br_defender_mix(ws, bet_p, br_call, br1);
    return 0.5 * (br0 + br1 - ev.ev_bettor - ev.ev_defender);
}

namespace {

[[nodiscard]] double preflop_equity(const LiveCombo& jammer, const LiveCombo& caller, std::mt19937& rng) {
    const std::vector<Card> hole{card_from_deck_index(jammer.card_a), card_from_deck_index(jammer.card_b)};
    return static_cast<double>(simulate_hand_outcome_vs_villain_holes(hole, {}, caller.card_a, caller.card_b,
                                                                     kPreflopMcTrials, rng, nullptr));
}

}  // namespace

PushFoldCfrResult cfr_heads_up_push_fold_solve(const SparseRange& jammer_range, const SparseRange& caller_range,
                                               double stack_bb, int iterations) {
    require_finite_positive(stack_bb, "stackBb");
    if (stack_bb <= 1.0) {
        throw std::invalid_argument("stackBb must be greater than 1");
    }
    const int T = clamp_iterations(iterations);
    const std::vector<Card> empty;
    const std::vector<LiveCombo> jammer = live_combos(jammer_range, empty, false);
    const std::vector<LiveCombo> caller = live_combos(caller_range, empty, false);
    const std::size_t nJ = jammer.size();
    const std::size_t nC = caller.size();
    std::vector<double> eq(nJ * nC, -1.0);
    std::mt19937 rng(0xC0FFEE);
    for (std::size_t i = 0; i < nJ; ++i) {
        for (std::size_t j = 0; j < nC; ++j) {
            if (overlap(jammer[i], caller[j])) {
                continue;
            }
            eq[i * nC + j] = preflop_equity(jammer[i], caller[j], rng);
        }
    }

    const double fold_sb = -0.5;
    const double jam_fold = 1.0;
    auto jam_call_ev = [&](double e) { return stack_bb * (2.0 * e - 1.0); };

    std::vector<std::vector<double>> r_jam(nJ, std::vector<double>(2, 0.0));
    std::vector<std::vector<double>> r_call(nC, std::vector<double>(2, 0.0));
    std::vector<double> sum_jam(nJ, 0.0);
    std::vector<double> sum_call(nC, 0.0);
    std::vector<double> jam_p(nJ, 0.5);
    std::vector<double> call_p(nC, 0.5);

    for (int t = 0; t < T; ++t) {
        for (std::size_t i = 0; i < nJ; ++i) {
            jam_p[i] = regret_matching_strategy(r_jam[i])[1];
        }
        for (std::size_t j = 0; j < nC; ++j) {
            call_p[j] = regret_matching_strategy(r_call[j])[1];
        }
        for (std::size_t i = 0; i < nJ; ++i) {
            sum_jam[i] += jam_p[i];
        }
        for (std::size_t j = 0; j < nC; ++j) {
            sum_call[j] += call_p[j];
        }
        for (std::size_t i = 0; i < nJ; ++i) {
            double z = 0.0;
            double v_jam = 0.0;
            for (std::size_t j = 0; j < nC; ++j) {
                const double e = eq[i * nC + j];
                if (e < 0.0) {
                    continue;
                }
                z += caller[j].weight;
                v_jam += caller[j].weight * ((1.0 - call_p[j]) * jam_fold + call_p[j] * jam_call_ev(e));
            }
            const double vj = z > 0.0 ? v_jam / z : jam_fold;
            const double v_sigma = (1.0 - jam_p[i]) * fold_sb + jam_p[i] * vj;
            r_jam[i][0] += fold_sb - v_sigma;
            r_jam[i][1] += vj - v_sigma;
        }
        for (std::size_t j = 0; j < nC; ++j) {
            double z = 0.0;
            double ev_call = 0.0;
            for (std::size_t i = 0; i < nJ; ++i) {
                const double e = eq[i * nC + j];
                if (e < 0.0) {
                    continue;
                }
                const double w = jammer[i].weight * jam_p[i];
                if (w <= 0.0) {
                    continue;
                }
                z += w;
                ev_call += w * (-jam_call_ev(e));
            }
            const double v_fold = 1.0;
            const double vc = z > 0.0 ? ev_call / z : v_fold;
            const double v_sigma = (1.0 - call_p[j]) * v_fold + call_p[j] * vc;
            r_call[j][0] += v_fold - v_sigma;
            r_call[j][1] += vc - v_sigma;
        }
    }
    for (std::size_t i = 0; i < nJ; ++i) {
        jam_p[i] = sum_jam[i] / static_cast<double>(T);
    }
    for (std::size_t j = 0; j < nC; ++j) {
        call_p[j] = sum_call[j] / static_cast<double>(T);
    }

    double z = 0.0;
    double ev_j = 0.0;
    for (std::size_t i = 0; i < nJ; ++i) {
        for (std::size_t j = 0; j < nC; ++j) {
            const double e = eq[i * nC + j];
            if (e < 0.0) {
                continue;
            }
            const double w = jammer[i].weight * caller[j].weight;
            z += w;
            const double leaf = (1.0 - jam_p[i]) * fold_sb +
                                jam_p[i] * ((1.0 - call_p[j]) * jam_fold + call_p[j] * jam_call_ev(e));
            ev_j += w * leaf;
        }
    }

    PushFoldCfrResult out;
    out.iterations = T;
    out.jam_freq = range_weighted_freq(jammer, jam_p);
    out.call_freq = range_weighted_freq(caller, call_p);
    out.ev_jammer = z > 0.0 ? ev_j / z : 0.0;
    out.ev_caller = -out.ev_jammer;
    write_mix_1326(out.jam_mix_1326, jammer, jam_p);
    write_mix_1326(out.call_mix_1326, caller, call_p);
    return out;
}

HuRiverCheckBetTreeResult solve_hu_river_check_bet_tree(double pot, double bet_size,
                                                        const SparseRange& bettor_range,
                                                        const SparseRange& defender_range,
                                                        const std::vector<Card>& board, int iterations,
                                                        int top_k) {
    HuRiverCheckBetTreeResult out;
    out.solve = cfr_river_bet_call_fold_solve(pot, bet_size, bettor_range, defender_range, board, iterations);
    const RiverWorkspace ws = make_river_workspace(pot, bet_size, bettor_range, defender_range, board);
    std::vector<double> bet_p;
    std::vector<double> call_p;
    fill_local_mix(out.solve.bet_mix_1326, ws.bettor, bet_p);
    fill_local_mix(out.solve.call_mix_1326, ws.defender, call_p);

    double bet_z[4]{};
    double bet_s[4]{};
    double call_z[4]{};
    double call_s[4]{};
    for (std::size_t i = 0; i < ws.bettor.size(); ++i) {
        const int b = class_bucket(ws.bettor[i].category);
        bet_z[b] += ws.bettor[i].weight;
        bet_s[b] += ws.bettor[i].weight * bet_p[i];
    }
    for (std::size_t j = 0; j < ws.defender.size(); ++j) {
        const int b = class_bucket(ws.defender[j].category);
        call_z[b] += ws.defender[j].weight;
        call_s[b] += ws.defender[j].weight * call_p[j];
    }
    auto avg = [](double s, double z) { return z > 0.0 ? s / z : 0.0; };
    out.classes.air_bet = avg(bet_s[0], bet_z[0]);
    out.classes.draw_bet = avg(bet_s[1], bet_z[1]);
    out.classes.made_bet = avg(bet_s[2], bet_z[2]);
    out.classes.strong_bet = avg(bet_s[3], bet_z[3]);
    out.classes.air_call = avg(call_s[0], call_z[0]);
    out.classes.draw_call = avg(call_s[1], call_z[1]);
    out.classes.made_call = avg(call_s[2], call_z[2]);
    out.classes.strong_call = avg(call_s[3], call_z[3]);

    std::vector<std::size_t> order(ws.bettor.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (bet_p[a] != bet_p[b]) {
            return bet_p[a] > bet_p[b];
        }
        return ws.bettor[a].weight > ws.bettor[b].weight;
    });
    const int k = std::min(std::max(0, top_k), static_cast<int>(order.size()));
    out.top_bet_combos.reserve(static_cast<std::size_t>(k));
    for (int t = 0; t < k; ++t) {
        const LiveCombo& c = ws.bettor[order[static_cast<std::size_t>(t)]];
        TopBetCombo row{};
        row.combo_index = c.index;
        row.card_a = c.card_a;
        row.card_b = c.card_b;
        row.bet_frequency = bet_p[order[static_cast<std::size_t>(t)]];
        row.weight = c.weight;
        out.top_bet_combos.push_back(row);
    }
    return out;
}

}  // namespace poker
