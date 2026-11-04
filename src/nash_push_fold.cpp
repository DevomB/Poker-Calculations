#include "poker/nash_push_fold.hpp"

#include "poker/equity_matrix.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/icm.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <utility>

namespace poker {
namespace {

constexpr int kHands = kNashHandCount;

void decode_hand169(int hand169, int& high_rank, int& low_rank, bool& suited) {
    const HandClass169 c = hand169_class(hand169);
    high_rank = c.high_rank;
    low_rank = c.low_rank;
    suited = c.suited;
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

double matchup_equity(int h0, int h1, int v0, int v1, int iterations, std::mt19937& rng) {
    std::array<int, 48> live{};
    int n = 0;
    const std::uint64_t dead = (1ULL << h0) | (1ULL << h1) | (1ULL << v0) | (1ULL << v1);
    for (int i = 0; i < 52; ++i) {
        if (((dead >> i) & 1ULL) == 0) {
            live[static_cast<std::size_t>(n++)] = i;
        }
    }
    std::uint8_t hr[7]{};
    std::uint8_t hs[7]{};
    std::uint8_t vr[7]{};
    std::uint8_t vs[7]{};
    hr[0] = static_cast<std::uint8_t>(h0 / 4);
    hs[0] = static_cast<std::uint8_t>(h0 % 4);
    hr[1] = static_cast<std::uint8_t>(h1 / 4);
    hs[1] = static_cast<std::uint8_t>(h1 % 4);
    vr[0] = static_cast<std::uint8_t>(v0 / 4);
    vs[0] = static_cast<std::uint8_t>(v0 % 4);
    vr[1] = static_cast<std::uint8_t>(v1 / 4);
    vs[1] = static_cast<std::uint8_t>(v1 % 4);
    double sum = 0.0;
    for (int t = 0; t < iterations; ++t) {
        for (int k = 0; k < 5; ++k) {
            const int j = k + static_cast<int>(rng() % static_cast<unsigned>(n - k));
            std::swap(live[static_cast<std::size_t>(k)], live[static_cast<std::size_t>(j)]);
            const int id = live[static_cast<std::size_t>(k)];
            hr[2 + k] = static_cast<std::uint8_t>(id / 4);
            hs[2 + k] = static_cast<std::uint8_t>(id % 4);
            vr[2 + k] = hr[2 + k];
            vs[2 + k] = hs[2 + k];
        }
        const int cmp = compare_seven_strength_fast(hr, hs, vr, vs);
        if (cmp > 0) {
            sum += 1.0;
        } else if (cmp == 0) {
            sum += 0.5;
        }
    }
    return sum / static_cast<double>(iterations);
}

void fill_hand169_equity_matrix(int iterations, std::uint32_t seed, std::vector<double>& out) {
    out.assign(static_cast<std::size_t>(kHands * kHands), 0.5);
    for (int i = 0; i < kHands; ++i) {
        int hi = 0;
        int lo = 0;
        bool suited = false;
        decode_hand169(i, hi, lo, suited);
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
            decode_hand169(j, vhi, vlo, vs);
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

/// The equity matrix for (iterations, seed), shared by solves on any thread. The caller gets an
/// immutable snapshot taken under the lock: a later rebuild for other settings swaps in a new
/// matrix instead of overwriting the one another solve is still reading.
std::shared_ptr<const std::vector<double>> cached_equity_matrix(int iterations, std::uint32_t seed) {
    static std::mutex mu;
    static int cached_iters = -1;
    static std::uint32_t cached_seed = 0;
    static std::shared_ptr<const std::vector<double>> cached;
    if (iterations < 1) {
        throw std::invalid_argument("equityIterations must be positive");
    }
    std::lock_guard<std::mutex> lock(mu);
    if (cached && cached_iters == iterations && cached_seed == seed) {
        return cached;
    }
    auto fresh = std::make_shared<std::vector<double>>();
    fill_hand169_equity_matrix(iterations, seed, *fresh);
    cached = std::move(fresh);
    cached_iters = iterations;
    cached_seed = seed;
    return cached;
}

double combo_weight_from_ranks(int high, int low, bool suited) {
    if (high == low) {
        return 6.0;
    }
    return suited ? 4.0 : 12.0;
}

const std::array<double, kHands>& combo_weights() {
    static const std::array<double, kHands> w = [] {
        std::array<double, kHands> out{};
        for (int h = 0; h < kHands; ++h) {
            int hi = 0;
            int lo = 0;
            bool suited = false;
            decode_hand169(h, hi, lo, suited);
            out[static_cast<std::size_t>(h)] = combo_weight_from_ranks(hi, lo, suited);
        }
        return out;
    }();
    return w;
}

double weight_sum(const std::array<double, kHands>& freq) {
    const auto& w = combo_weights();
    double s = 0.0;
    for (int i = 0; i < kHands; ++i) {
        s += w[static_cast<std::size_t>(i)] * freq[static_cast<std::size_t>(i)];
    }
    return s;
}

double equity_vs_freq(int hand, const std::array<double, kHands>& freq, const std::vector<double>& matrix) {
    const auto& w = combo_weights();
    double num = 0.0;
    double den = 0.0;
    const std::size_t row = static_cast<std::size_t>(hand) * static_cast<std::size_t>(kHands);
    for (int j = 0; j < kHands; ++j) {
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

void mix_avg(std::array<double, kHands>& avg, const std::array<double, kHands>& br, int iter) {
    const double n = static_cast<double>(iter);
    for (int i = 0; i < kHands; ++i) {
        avg[static_cast<std::size_t>(i)] =
            (avg[static_cast<std::size_t>(i)] * n + br[static_cast<std::size_t>(i)]) / (n + 1.0);
    }
}

void validate_spec(const NashPushFoldSpec& spec) {
    if (!(spec.big_blind > 0.0) || !std::isfinite(spec.big_blind)) {
        throw std::invalid_argument("bigBlind must be positive");
    }
    if (spec.small_blind < 0.0 || spec.ante < 0.0 || spec.hero_posted < 0.0 || spec.villain_posted < 0.0) {
        throw std::invalid_argument("blinds/ante/posted must be non-negative");
    }
    if (!(spec.hero_stack > spec.hero_posted) || !(spec.villain_stack > spec.villain_posted)) {
        throw std::invalid_argument("stack must exceed posted blind");
    }
    if (spec.max_iterations < 1) {
        throw std::invalid_argument("maxIterations must be positive");
    }
    if (spec.use_icm) {
        if (spec.payouts.empty()) {
            throw std::invalid_argument("ICM solve requires payouts[]");
        }
        const std::size_t n = 2 + spec.other_stacks.size();
        if (spec.payouts.size() > n) {
            throw std::invalid_argument("payouts longer than player count");
        }
    }
}

int clamp_iters(int n) {
    return std::max(1, std::min(n, kNashMaxIterations));
}

struct IcmPair {
    double hero{0.0};
    double villain{0.0};
};

IcmPair icm_for_stacks(double hero, double villain, const std::vector<double>& others,
                       const std::vector<double>& payouts) {
    std::vector<double> stacks;
    stacks.reserve(2 + others.size());
    stacks.push_back(hero);
    stacks.push_back(villain);
    stacks.insert(stacks.end(), others.begin(), others.end());
    // Prizes for every seat (missing places pay 0); a busted seat takes the bottom prize it can no
    // longer avoid, so each terminal conserves the prize pool.
    std::vector<double> prizes = payouts;
    prizes.resize(stacks.size(), 0.0);
    const auto ev = icm_expected_payouts_allowing_busts(stacks, prizes);
    return IcmPair{ev[0], ev[1]};
}

/// Each player's value at each way the hand can end: final stacks minus starting stacks in chip
/// mode, ICM prize EV otherwise.
struct Terminals {
    IcmPair fold{};
    IcmPair jam_fold{};
    IcmPair hero_wins{};
    IcmPair villain_wins{};
};

Terminals build_terminals(const NashPushFoldSpec& spec) {
    const NashTerminalStacks t = nash_terminal_stacks(spec.hero_stack, spec.villain_stack, spec.hero_posted,
                                                      spec.villain_posted, spec.ante);
    const auto value = [&](const std::array<double, 2>& st) {
        if (spec.use_icm) {
            return icm_for_stacks(st[0], st[1], spec.other_stacks, spec.payouts);
        }
        return IcmPair{st[0] - spec.hero_stack, st[1] - spec.villain_stack};
    };
    return Terminals{value(t.fold), value(t.jam_fold), value(t.hero_wins), value(t.villain_wins)};
}

/// Hero's value from jamming hand with equity `eq` against the calls it meets (`p_call`).
double jam_value(double eq, double p_call, const Terminals& t) {
    const double show = eq * t.hero_wins.hero + (1.0 - eq) * t.villain_wins.hero;
    return (1.0 - p_call) * t.jam_fold.hero + p_call * show;
}

/// Villain's value from calling with equity `eq_villain` against the jams it faces.
double call_value(double eq_villain, const Terminals& t) {
    return eq_villain * t.villain_wins.villain + (1.0 - eq_villain) * t.hero_wins.villain;
}

}  // namespace

int nash_hand169_index(int high_rank, int low_rank, bool suited) {
    return hand169_index(high_rank, low_rank, suited);
}

int nash_hand169_from_notation(const std::string& notation_raw) {
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
    if (i == s.size()) {
        if (r1 != r2) {
            throw std::invalid_argument("non-pair notation needs s/o suffix");
        }
        return nash_hand169_index(r1, r2, false);
    }
    if (i + 1 != s.size()) {
        throw std::invalid_argument("invalid hand notation");
    }
    const char suf = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    if (r1 == r2) {
        throw std::invalid_argument("pair notation must omit s/o");
    }
    if (suf == 's') {
        return nash_hand169_index(r1, r2, true);
    }
    if (suf == 'o') {
        return nash_hand169_index(r1, r2, false);
    }
    throw std::invalid_argument("hand notation suffix must be s or o");
}

double nash_combo_weight(int hand169) {
    return combo_weights()[static_cast<std::size_t>(hand169)];
}

NashTerminalStacks nash_terminal_stacks(double hero_stack, double villain_stack, double hero_posted,
                                        double villain_posted, double ante) {
    for (double x : {hero_stack, villain_stack, hero_posted, villain_posted, ante}) {
        if (!std::isfinite(x) || x < 0.0) {
            throw std::invalid_argument("stacks, posted blinds, and ante must be finite and non-negative");
        }
    }
    if (!(hero_stack > hero_posted) || !(villain_stack > villain_posted)) {
        throw std::invalid_argument("stack must exceed posted blind");
    }
    // Stacks include the posted blinds; the ante is dead money in the pot. A called all-in commits
    // the smaller total stack M from each player (the bigger stack's excess is returned), so the
    // pot is 2M + ante and each player's call cost is M minus what they already posted.
    NashTerminalStacks t;
    t.matched = std::min(hero_stack, villain_stack);
    t.called_pot = 2.0 * t.matched + ante;
    const double pot_now = hero_posted + villain_posted + ante;
    t.fold = {hero_stack - hero_posted, villain_stack - villain_posted + pot_now};
    t.jam_fold = {hero_stack - hero_posted + pot_now, villain_stack - villain_posted};
    t.hero_wins = {hero_stack - t.matched + t.called_pot, villain_stack - t.matched};
    t.villain_wins = {hero_stack - t.matched, villain_stack - t.matched + t.called_pot};
    return t;
}

NashJamCallResult nash_heads_up_jam_call_solve(const NashPushFoldSpec& spec) {
    validate_spec(spec);
    const int iters = clamp_iters(spec.max_iterations);
    const std::shared_ptr<const std::vector<double>> snapshot =
        cached_equity_matrix(spec.equity_iterations, spec.equity_seed);
    const std::vector<double>& matrix = *snapshot;
    const Terminals term = build_terminals(spec);
    const double fold_hero = term.fold.hero;
    const double fold_villain = term.jam_fold.villain;
    const auto& w = combo_weights();
    std::array<double, kHands> ones{};
    ones.fill(1.0);
    const double w_all = weight_sum(ones);

    std::array<double, kHands> jam_avg{};
    std::array<double, kHands> call_avg{};
    jam_avg.fill(0.5);
    call_avg.fill(0.5);

    std::array<double, kHands> jam_br{};
    std::array<double, kHands> call_br{};

    for (int iter = 0; iter < iters; ++iter) {
        const double p_call = weight_sum(call_avg) / w_all;
        for (int i = 0; i < kHands; ++i) {
            const double eq = equity_vs_freq(i, call_avg, matrix);
            jam_br[static_cast<std::size_t>(i)] = br_from_ev(jam_value(eq, p_call, term), fold_hero, spec.tolerance);
        }
        for (int j = 0; j < kHands; ++j) {
            const double eq = equity_vs_freq(j, jam_avg, matrix);
            call_br[static_cast<std::size_t>(j)] = br_from_ev(call_value(eq, term), fold_villain, spec.tolerance);
        }
        mix_avg(jam_avg, jam_br, iter);
        mix_avg(call_avg, call_br, iter);
    }

    NashJamCallResult out;
    out.jam = jam_avg;
    out.call = call_avg;
    out.iterations = iters;
    // Root values for both players over the same joint deal of hand classes (combo-weighted), so
    // they add up to what the terminals conserve: the ante in chip mode, the prize pool less the
    // other seats' share under ICM.
    double hero = 0.0;
    double villain = 0.0;
    for (int i = 0; i < kHands; ++i) {
        const double pi = w[static_cast<std::size_t>(i)] / w_all;
        const double jam = jam_avg[static_cast<std::size_t>(i)];
        double h_jam = 0.0;
        double v_jam = 0.0;
        for (int j = 0; j < kHands; ++j) {
            const double pj = w[static_cast<std::size_t>(j)] / w_all;
            const double call = call_avg[static_cast<std::size_t>(j)];
            const double e = matrix[static_cast<std::size_t>(i) * kHands + static_cast<std::size_t>(j)];
            h_jam += pj * ((1.0 - call) * term.jam_fold.hero +
                           call * (e * term.hero_wins.hero + (1.0 - e) * term.villain_wins.hero));
            v_jam += pj * ((1.0 - call) * term.jam_fold.villain +
                           call * (e * term.hero_wins.villain + (1.0 - e) * term.villain_wins.villain));
        }
        hero += pi * ((1.0 - jam) * term.fold.hero + jam * h_jam);
        villain += pi * ((1.0 - jam) * term.fold.villain + jam * v_jam);
    }
    out.hero_ev = hero;
    out.villain_ev = villain;
    return out;
}


namespace {

std::vector<double> stack_grid_bb(double max_stack_bb) {
    const double cap = std::max(2.0, std::min(max_stack_bb, 50.0));
    const double raw[] = {1.5, 2, 2.5, 3, 3.5, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20, 25, 30, 40, 50};
    std::vector<double> g;
    for (double x : raw) {
        if (x <= cap + 1e-9) {
            g.push_back(x);
        }
    }
    if (g.empty() || g.back() + 1e-9 < cap) {
        g.push_back(cap);
    }
    return g;
}

NashPushFoldSpec spec_at_stack_bb(const NashPushFoldSpec& spec, double stack_bb) {
    NashPushFoldSpec s = spec;
    s.hero_stack = stack_bb * spec.big_blind;
    s.villain_stack = stack_bb * spec.big_blind;
    s.hero_posted = spec.small_blind;
    s.villain_posted = spec.big_blind;
    s.use_icm = false;
    return s;
}

std::array<double, kHands> thresholds_from_grid(const NashPushFoldSpec& spec, double max_stack_bb,
                                                bool jam_side) {
    const auto grid = stack_grid_bb(max_stack_bb);
    std::vector<std::array<double, kHands>> freqs;
    freqs.reserve(grid.size());
    for (double sb : grid) {
        const auto r = nash_heads_up_jam_call_solve(spec_at_stack_bb(spec, sb));
        freqs.push_back(jam_side ? r.jam : r.call);
    }
    std::array<double, kHands> out{};
    for (int h = 0; h < kHands; ++h) {
        double thr = 0.0;
        for (std::size_t g = 0; g < grid.size(); ++g) {
            const double f = freqs[g][static_cast<std::size_t>(h)];
            if (f >= 0.5) {
                thr = grid[g];
                if (g + 1 < grid.size() && freqs[g + 1][static_cast<std::size_t>(h)] < 0.5) {
                    const double f1 = f;
                    const double f2 = freqs[g + 1][static_cast<std::size_t>(h)];
                    const double t = (f1 - 0.5) / (f1 - f2);
                    thr = grid[g] + t * (grid[g + 1] - grid[g]);
                    break;
                }
            }
        }
        out[static_cast<std::size_t>(h)] = thr;
    }
    return out;
}

}  // namespace

std::array<double, kNashHandCount> nash_jam_threshold_stack_bb(const NashPushFoldSpec& spec,
                                                               double max_stack_bb) {
    return thresholds_from_grid(spec, max_stack_bb, true);
}

std::array<double, kNashHandCount> nash_call_threshold_stack_bb(const NashPushFoldSpec& spec,
                                                                double max_stack_bb) {
    return thresholds_from_grid(spec, max_stack_bb, false);
}

double nash_indifference_stack_bb(int hand169, const NashPushFoldSpec& spec, double max_stack_bb) {
    if (hand169 < 0 || hand169 > 168) {
        throw std::invalid_argument("hand169 out of range");
    }
    const double hi0 = std::max(2.0, std::min(max_stack_bb, 50.0));
    double lo = 1.5;
    double hi = hi0;
    const auto warm = nash_heads_up_jam_call_solve(spec_at_stack_bb(spec, lo));
    if (warm.jam[static_cast<std::size_t>(hand169)] < 0.5) {
        return 0.0;
    }
    for (int step = 0; step < 18; ++step) {
        const double mid = 0.5 * (lo + hi);
        const auto r = nash_heads_up_jam_call_solve(spec_at_stack_bb(spec, mid));
        if (r.jam[static_cast<std::size_t>(hand169)] >= 0.5) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

}  // namespace poker
