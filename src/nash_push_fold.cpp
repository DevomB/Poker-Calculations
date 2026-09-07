#include "poker/nash_push_fold.hpp"

#include "poker/fast_evaluator.hpp"
#include "poker/icm.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <mutex>
#include <random>
#include <stdexcept>
#include <utility>

namespace poker {
namespace {

constexpr int kHands = kNashHandCount;

void decode_hand169(int hand169, int& high_rank, int& low_rank, bool& suited) {
    if (hand169 < 0 || hand169 > 168) {
        throw std::invalid_argument("nash hand169 out of range");
    }
    int idx = 0;
    for (int i = 0; i < 13; ++i) {
        for (int j = i; j < 13; ++j) {
            if (i == j) {
                if (idx == hand169) {
                    high_rank = i;
                    low_rank = j;
                    suited = false;
                    return;
                }
                ++idx;
            } else {
                if (idx == hand169) {
                    high_rank = j;
                    low_rank = i;
                    suited = true;
                    return;
                }
                ++idx;
                if (idx == hand169) {
                    high_rank = j;
                    low_rank = i;
                    suited = false;
                    return;
                }
                ++idx;
            }
        }
    }
    throw std::invalid_argument("nash hand169 decode failed");
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
        cached.size() == static_cast<std::size_t>(kHands * kHands)) {
        return cached;
    }
    fill_hand169_equity_matrix(iterations, seed, cached);
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

struct ChipSpot {
    double pot{0.0};
    double eff{0.0};
};

ChipSpot chip_spot(double hero_stack, double villain_stack, double hero_posted, double villain_posted,
                   double ante) {
    ChipSpot s;
    s.pot = hero_posted + villain_posted + ante;
    const double rh = hero_stack - hero_posted;
    const double rv = villain_stack - villain_posted;
    s.eff = std::min(rh, rv);
    if (!(s.pot > 0.0) || !(s.eff > 0.0)) {
        throw std::invalid_argument("pot and effective remaining stack must be positive");
    }
    return s;
}

double chip_called_ev(double eq, const ChipSpot& spot) {
    return eq * spot.pot + (2.0 * eq - 1.0) * spot.eff;
}

double chip_jam_ev(double eq, double p_call, const ChipSpot& spot) {
    const double p_fold = 1.0 - p_call;
    return p_fold * spot.pot + p_call * chip_called_ev(eq, spot);
}

struct IcmPair {
    double hero{0.0};
    double villain{0.0};
};

IcmPair icm_for_stacks(double hero, double villain, const std::vector<double>& others,
                       const std::vector<double>& payouts) {
    std::vector<double> stacks;
    stacks.reserve(2 + others.size());
    stacks.push_back(std::max(hero, 1e-6));
    stacks.push_back(std::max(villain, 1e-6));
    stacks.insert(stacks.end(), others.begin(), others.end());
    const auto ev = icm_expected_payouts(stacks, payouts);
    IcmPair p;
    p.hero = ev[0];
    p.villain = ev.size() > 1 ? ev[1] : 0.0;
    return p;
}

struct IcmTerminals {
    IcmPair fold{};
    IcmPair jam_fold{};
    IcmPair hero_wins{};
    IcmPair villain_wins{};
};

IcmTerminals build_icm_terminals(const NashPushFoldSpec& spec, const ChipSpot& spot) {
    IcmTerminals t;
    const double h_behind = spec.hero_stack - spec.hero_posted;
    const double v_behind = spec.villain_stack - spec.villain_posted;
    t.fold = icm_for_stacks(h_behind, v_behind + spot.pot, spec.other_stacks, spec.payouts);
    t.jam_fold = icm_for_stacks(h_behind + spot.pot, v_behind, spec.other_stacks, spec.payouts);
    t.hero_wins =
        icm_for_stacks(h_behind - spot.eff + spot.pot + 2.0 * spot.eff, v_behind - spot.eff, spec.other_stacks,
                       spec.payouts);
    t.villain_wins =
        icm_for_stacks(h_behind - spot.eff, v_behind - spot.eff + spot.pot + 2.0 * spot.eff, spec.other_stacks,
                       spec.payouts);
    return t;
}

double icm_jam_ev(double eq, double p_call, const IcmTerminals& t) {
    const double show = eq * t.hero_wins.hero + (1.0 - eq) * t.villain_wins.hero;
    return (1.0 - p_call) * t.jam_fold.hero + p_call * show;
}

double icm_call_ev(double eq, const IcmTerminals& t) {
    return eq * t.villain_wins.villain + (1.0 - eq) * t.hero_wins.villain;
}

}  // namespace

int nash_hand169_index(int high_rank, int low_rank, bool suited) {
    if (high_rank < 0 || high_rank > 12 || low_rank < 0 || low_rank > 12) {
        throw std::invalid_argument("rank out of range");
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
    for (int i = 0; i < 13; ++i) {
        for (int j = i; j < 13; ++j) {
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
    throw std::invalid_argument("hand169 index not found");
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

NashJamCallResult nash_heads_up_jam_call_solve(const NashPushFoldSpec& spec) {
    validate_spec(spec);
    const int iters = clamp_iters(spec.max_iterations);
    const auto& matrix = cached_equity_matrix(spec.equity_iterations, spec.equity_seed);
    const ChipSpot spot =
        chip_spot(spec.hero_stack, spec.villain_stack, spec.hero_posted, spec.villain_posted, spec.ante);
    const IcmTerminals icm = spec.use_icm ? build_icm_terminals(spec, spot) : IcmTerminals{};
    const double fold_hero = spec.use_icm ? icm.fold.hero : 0.0;
    const double fold_villain = spec.use_icm ? icm.jam_fold.villain : 0.0;
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
        const double call_mass = weight_sum(call_avg);
        const double p_call = call_mass / w_all;
        for (int i = 0; i < kHands; ++i) {
            const double eq = equity_vs_freq(i, call_avg, matrix);
            const double ev =
                spec.use_icm ? icm_jam_ev(eq, p_call, icm) : chip_jam_ev(eq, p_call, spot);
            jam_br[static_cast<std::size_t>(i)] = br_from_ev(ev, fold_hero, spec.tolerance);
        }
        for (int j = 0; j < kHands; ++j) {
            const double eq = equity_vs_freq(j, jam_avg, matrix);
            const double ev = spec.use_icm ? icm_call_ev(eq, icm) : chip_called_ev(eq, spot);
            call_br[static_cast<std::size_t>(j)] = br_from_ev(ev, fold_villain, spec.tolerance);
        }
        mix_avg(jam_avg, jam_br, iter);
        mix_avg(call_avg, call_br, iter);
    }

    NashJamCallResult out;
    out.jam = jam_avg;
    out.call = call_avg;
    out.iterations = iters;
    const double call_mass = weight_sum(call_avg);
    const double p_call = call_mass / w_all;
    double hero = 0.0;
    double villain = 0.0;
    for (int i = 0; i < kHands; ++i) {
        const double p = w[static_cast<std::size_t>(i)] / w_all;
        const double eq = equity_vs_freq(i, call_avg, matrix);
        const double ev_j = spec.use_icm ? icm_jam_ev(eq, p_call, icm) : chip_jam_ev(eq, p_call, spot);
        hero += p * (jam_avg[static_cast<std::size_t>(i)] * ev_j +
                     (1.0 - jam_avg[static_cast<std::size_t>(i)]) * fold_hero);
    }
    if (spec.use_icm) {
        for (int j = 0; j < kHands; ++j) {
            const double p = w[static_cast<std::size_t>(j)] / w_all;
            const double eq = equity_vs_freq(j, jam_avg, matrix);
            const double ev_c = icm_call_ev(eq, icm);
            villain += p * (call_avg[static_cast<std::size_t>(j)] * ev_c +
                            (1.0 - call_avg[static_cast<std::size_t>(j)]) * fold_villain);
        }
    } else {
        villain = -hero;
    }
    out.hero_ev = hero;
    out.villain_ev = villain;
    return out;
}

NashMultiwayResult nash_multiway_shove_call(const NashPushFoldSpec& spec,
                                            const std::vector<double>& caller_stacks) {
    if (caller_stacks.empty() || caller_stacks.size() > 8) {
        throw std::invalid_argument("callerStacks length must be 1..8");
    }
    for (double s : caller_stacks) {
        if (!(s > 0.0) || !std::isfinite(s)) {
            throw std::invalid_argument("caller stack must be positive");
        }
    }
    NashPushFoldSpec base = spec;
    if (base.hero_posted < 0.0) {
        base.hero_posted = 0.0;
    }
    if (!(base.hero_stack > base.hero_posted)) {
        throw std::invalid_argument("shover stack must exceed posted");
    }
    const int iters = clamp_iters(spec.max_iterations);
    const auto& matrix = cached_equity_matrix(spec.equity_iterations, spec.equity_seed);
    const double dead = spec.small_blind + spec.big_blind + spec.ante;
    if (!(dead > 0.0)) {
        throw std::invalid_argument("dead pot (sb+bb+ante) must be positive");
    }
    const double shover_behind = spec.hero_stack - spec.hero_posted;
    const auto& w = combo_weights();
    std::array<double, kHands> ones{};
    ones.fill(1.0);
    const double w_all = weight_sum(ones);

    const int n = static_cast<int>(caller_stacks.size());
    std::array<double, kHands> jam_avg{};
    jam_avg.fill(0.5);
    std::vector<std::array<double, kHands>> call_avg(static_cast<std::size_t>(n));
    for (auto& c : call_avg) {
        c.fill(0.5);
    }

    auto caller_others = [&](int skip) {
        std::vector<double> o = spec.other_stacks;
        for (int k = 0; k < n; ++k) {
            if (k != skip) {
                o.push_back(caller_stacks[static_cast<std::size_t>(k)]);
            }
        }
        return o;
    };

    for (int iter = 0; iter < iters; ++iter) {
        std::array<double, kHands> jam_br{};
        std::vector<std::array<double, kHands>> call_br(static_cast<std::size_t>(n));
        for (int k = 0; k < n; ++k) {
            const double eff = std::min(shover_behind, caller_stacks[static_cast<std::size_t>(k)]);
            ChipSpot spot{dead, eff};
            IcmTerminals icm{};
            if (spec.use_icm) {
                NashPushFoldSpec one = spec;
                one.villain_stack = caller_stacks[static_cast<std::size_t>(k)];
                one.villain_posted = 0.0;
                one.other_stacks = caller_others(k);
                icm = build_icm_terminals(one, spot);
            }
            for (int j = 0; j < kHands; ++j) {
                const double eq = equity_vs_freq(j, jam_avg, matrix);
                const double ev = spec.use_icm ? icm_call_ev(eq, icm) : chip_called_ev(eq, spot);
                const double fold_v = spec.use_icm ? icm.jam_fold.villain : 0.0;
                call_br[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)] =
                    br_from_ev(ev, fold_v, spec.tolerance);
            }
        }

        for (int i = 0; i < kHands; ++i) {
            double p_alive = 1.0;
            double ev = 0.0;
            double fold_ev = 0.0;
            if (spec.use_icm) {
                NashPushFoldSpec one = spec;
                one.villain_stack = caller_stacks[0];
                one.villain_posted = 0.0;
                one.other_stacks = caller_others(0);
                const double eff0 = std::min(shover_behind, caller_stacks[0]);
                fold_ev = build_icm_terminals(one, ChipSpot{dead, eff0}).fold.hero;
            }
            for (int k = 0; k < n; ++k) {
                const double p_call = weight_sum(call_avg[static_cast<std::size_t>(k)]) / w_all;
                const double eff = std::min(shover_behind, caller_stacks[static_cast<std::size_t>(k)]);
                const ChipSpot spot{dead, eff};
                const double eq = equity_vs_freq(i, call_avg[static_cast<std::size_t>(k)], matrix);
                double leaf = chip_called_ev(eq, spot);
                if (spec.use_icm) {
                    NashPushFoldSpec one = spec;
                    one.villain_stack = caller_stacks[static_cast<std::size_t>(k)];
                    one.villain_posted = 0.0;
                    one.other_stacks = caller_others(k);
                    const IcmTerminals icm = build_icm_terminals(one, spot);
                    leaf = eq * icm.hero_wins.hero + (1.0 - eq) * icm.villain_wins.hero;
                }
                ev += p_alive * p_call * leaf;
                p_alive *= (1.0 - p_call);
            }
            if (spec.use_icm) {
                NashPushFoldSpec one = spec;
                one.villain_stack = caller_stacks[0];
                one.villain_posted = 0.0;
                one.other_stacks = caller_others(0);
                const double eff0 = std::min(shover_behind, caller_stacks[0]);
                ev += p_alive * build_icm_terminals(one, ChipSpot{dead, eff0}).jam_fold.hero;
            } else {
                ev += p_alive * dead;
            }
            jam_br[static_cast<std::size_t>(i)] = br_from_ev(ev, fold_ev, spec.tolerance);
        }
        mix_avg(jam_avg, jam_br, iter);
        for (int k = 0; k < n; ++k) {
            mix_avg(call_avg[static_cast<std::size_t>(k)], call_br[static_cast<std::size_t>(k)], iter);
        }
    }

    NashMultiwayResult out;
    out.jam = jam_avg;
    out.calls = std::move(call_avg);
    out.iterations = iters;
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
