#include "binding_strategy_tools.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "binding_state.hpp"

#include "poker/exact_equity.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/poker_math.hpp"
#include "poker/range.hpp"
#include "poker/range_equity.hpp"
#include "poker/strategy.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;

namespace {

constexpr std::size_t kCombos = 1326;

double clamp01(double x) {
    if (!std::isfinite(x)) {
        return 0.0;
    }
    return std::clamp(x, 0.0, 1.0);
}

int deck_index(const poker::Card& c) {
    return static_cast<int>(c.rank()) * 4 + static_cast<int>(c.suit());
}

int combo_index(int a, int b) {
    if (a == b) {
        return -1;
    }
    if (a > b) {
        std::swap(a, b);
    }
    if (a < 0 || b > 51) {
        return -1;
    }
    return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

std::pair<int, int> combo_cards(int idx) {
    int cur = 0;
    for (int a = 0; a < 52; ++a) {
        const int width = 51 - a;
        if (idx < cur + width) {
            return {a, a + 1 + (idx - cur)};
        }
        cur += width;
    }
    return {-1, -1};
}

bool read_range_dense(const Napi::Value& v, std::vector<double>& out, std::string* err) {
    out.assign(kCombos, 0.0);
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_float64_array || ta.ElementLength() != kCombos) {
            if (err) {
                *err = "range must be Float64Array(1326) or { indices, weights }";
            }
            return false;
        }
        std::memcpy(out.data(), ta.ArrayBuffer().Data(), kCombos * sizeof(double));
        for (double& w : out) {
            if (!std::isfinite(w) || w < 0.0) {
                w = 0.0;
            }
        }
        return true;
    }
    if (!v.IsObject()) {
        if (err) {
            *err = "range must be Float64Array(1326) or { indices, weights }";
        }
        return false;
    }
    const Napi::Object o = v.As<Napi::Object>();
    if (!o.Has("indices") || !o.Has("weights")) {
        if (err) {
            *err = "range object needs indices and weights";
        }
        return false;
    }
    std::vector<double> weights;
    if (!read_f64_vector(o.Get("weights"), "weights", weights, err)) {
        return false;
    }
    std::vector<int> indices;
    const Napi::Value iv = o.Get("indices");
    if (iv.IsTypedArray()) {
        const Napi::TypedArray ta = iv.As<Napi::TypedArray>();
        indices.resize(ta.ElementLength());
        if (ta.TypedArrayType() == napi_int32_array) {
            std::memcpy(indices.data(), ta.ArrayBuffer().Data(), indices.size() * sizeof(std::int32_t));
        } else if (ta.TypedArrayType() == napi_uint32_array) {
            const auto* p = static_cast<const std::uint32_t*>(ta.ArrayBuffer().Data());
            for (std::size_t i = 0; i < indices.size(); ++i) {
                indices[i] = static_cast<int>(p[i]);
            }
        } else {
            if (err) {
                *err = "indices must be Int32Array, Uint32Array, or number[]";
            }
            return false;
        }
    } else if (iv.IsArray()) {
        const Napi::Array a = iv.As<Napi::Array>();
        indices.reserve(a.Length());
        for (uint32_t i = 0; i < a.Length(); ++i) {
            if (!a.Get(i).IsNumber()) {
                if (err) {
                    *err = "indices must contain numbers";
                }
                return false;
            }
            indices.push_back(a.Get(i).As<Napi::Number>().Int32Value());
        }
    } else {
        if (err) {
            *err = "indices must be Int32Array, Uint32Array, or number[]";
        }
        return false;
    }
    if (indices.size() % 2 != 0 || weights.size() != indices.size() / 2) {
        if (err) {
            *err = "range indices must contain two deck ids per weight";
        }
        return false;
    }
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const int idx = combo_index(indices[i * 2], indices[i * 2 + 1]);
        const double w = weights[i];
        if (idx >= 0 && std::isfinite(w) && w > 0.0) {
            out[static_cast<std::size_t>(idx)] += w;
        }
    }
    return true;
}

Napi::Value write_dense(Napi::Env env, const std::vector<double>& data) {
    Napi::ArrayBuffer buf = Napi::ArrayBuffer::New(env, data.size() * sizeof(double));
    std::memcpy(buf.Data(), data.data(), data.size() * sizeof(double));
    return Napi::Float64Array::New(env, data.size(), buf, 0);
}

double sum_positive(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v) {
        if (std::isfinite(x) && x > 0.0) {
            s += x;
        }
    }
    return s;
}

std::vector<double> normalized(std::vector<double> v) {
    const double s = sum_positive(v);
    if (s <= 0.0) {
        return v;
    }
    for (double& x : v) {
        x = (std::isfinite(x) && x > 0.0) ? x / s : 0.0;
    }
    return v;
}

std::uint64_t dead_mask_from_cards(const std::vector<poker::Card>& cards) {
    std::uint64_t mask = 0;
    for (const auto& c : cards) {
        mask |= std::uint64_t{1} << deck_index(c);
    }
    return mask;
}

std::uint64_t dead_mask_from_groups(const std::vector<poker::Card>& a, const std::vector<poker::Card>& b) {
    return dead_mask_from_cards(a) | dead_mask_from_cards(b);
}

poker::SparseRange sparse_from_dense(const std::vector<double>& dense, std::uint64_t dead) {
    return poker::sparse_range_from_dense1326(dense.data(), dense.size(), dead);
}

std::string combo_notation(int a, int b) {
    static constexpr char ranks[] = "23456789TJQKA";
    int ra = a / 4;
    int rb = b / 4;
    if (ra == rb) {
        std::string s;
        s.push_back(ranks[ra]);
        s.push_back(ranks[rb]);
        return s;
    }
    bool suited = (a % 4) == (b % 4);
    if (ra < rb) {
        std::swap(ra, rb);
    }
    std::string s;
    s.push_back(ranks[ra]);
    s.push_back(ranks[rb]);
    s.push_back(suited ? 's' : 'o');
    return s;
}

bool notation_to_combos(const std::string& notation, std::vector<int>& indices) {
    static const std::string ranks = "23456789TJQKA";
    if (notation.size() < 2 || notation.size() > 3) {
        return false;
    }
    const auto p0 = ranks.find(static_cast<char>(std::toupper(static_cast<unsigned char>(notation[0]))));
    const auto p1 = ranks.find(static_cast<char>(std::toupper(static_cast<unsigned char>(notation[1]))));
    if (p0 == std::string::npos || p1 == std::string::npos) {
        return false;
    }
    const int r0 = static_cast<int>(p0);
    const int r1 = static_cast<int>(p1);
    if (r0 == r1) {
        for (int s0 = 0; s0 < 4; ++s0) {
            for (int s1 = s0 + 1; s1 < 4; ++s1) {
                indices.push_back(combo_index(r0 * 4 + s0, r1 * 4 + s1));
            }
        }
        return notation.size() == 2;
    }
    if (notation.size() != 3 || (notation[2] != 's' && notation[2] != 'o')) {
        return false;
    }
    if (notation[2] == 's') {
        for (int s = 0; s < 4; ++s) {
            indices.push_back(combo_index(r0 * 4 + s, r1 * 4 + s));
        }
    } else {
        for (int s0 = 0; s0 < 4; ++s0) {
            for (int s1 = 0; s1 < 4; ++s1) {
                if (s0 != s1) {
                    indices.push_back(combo_index(r0 * 4 + s0, r1 * 4 + s1));
                }
            }
        }
    }
    return true;
}

double equity_vs_range_safe(const std::vector<poker::Card>& hero, const std::vector<poker::Card>& board,
                            const std::vector<double>& range) {
    try {
        const auto sparse = sparse_from_dense(range, dead_mask_from_groups(hero, board));
        return poker::exact_hu_equity_vs_range(hero, board, sparse);
    } catch (...) {
        return 0.0;
    }
}

Napi::Object legal_summary_from_state(Napi::Env env, const poker::PokerGameState& state) {
    Napi::Object o = Napi::Object::New(env);
    int idx = state.acting_index;
    if (idx < 0 && !state.players.empty()) {
        idx = 0;
    }
    const bool valid = idx >= 0 && idx < static_cast<int>(state.players.size());
    const int committed = valid ? state.players[static_cast<std::size_t>(idx)].committed_this_street : 0;
    const int stack = valid ? state.players[static_cast<std::size_t>(idx)].stack : 0;
    const int to_call = std::max(0, state.current_bet - committed);
    const int min_raise = state.current_bet + std::max(state.last_raise_increment, state.big_blind);
    o.Set("canFold", to_call > 0);
    o.Set("canCheck", to_call == 0);
    o.Set("canCall", to_call > 0 && stack > 0);
    o.Set("canRaise", stack > to_call);
    o.Set("toCall", to_call);
    o.Set("minRaiseTo", min_raise);
    o.Set("maxRaiseTo", committed + stack);
    o.Set("actingIndex", idx);
    return o;
}

Napi::Object config_to_js(Napi::Env env, const poker::BotConfig& cfg) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("aggressionThreshold", cfg.aggression_threshold);
    o.Set("riskTolerance", cfg.risk_tolerance);
    o.Set("monteCarloSimulations", cfg.monte_carlo_simulations);
    o.Set("monteCarloVillains", cfg.monte_carlo_villains);
    o.Set("raisePotFraction", cfg.raise_pot_fraction);
    o.Set("opponentAggressionWeight", cfg.opponent_aggression_weight);
    o.Set("rngSeed", cfg.rng_seed);
    return o;
}

Napi::Object decision_to_js(Napi::Env env, const poker::Decision& d) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("action", poker_bind::action_name(d.action));
    o.Set("raiseBy", d.raise_by);
    return o;
}

double get_number(const Napi::CallbackInfo& info, std::size_t idx, double fallback = 0.0) {
    if (info.Length() <= idx || !info[idx].IsNumber()) {
        return fallback;
    }
    return info[idx].As<Napi::Number>().DoubleValue();
}

}  // namespace

Napi::Value NormalizeSparseRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "normalizeSparseRange(range)" : err);
    }
    return write_dense(env, normalized(r));
}

Napi::Value PruneRangeByMinWeight(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 2 || !info[1].IsNumber() || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "pruneRangeByMinWeight(range, minWeight)" : err);
    }
    const double min_w = info[1].As<Napi::Number>().DoubleValue();
    std::vector<int> indices;
    std::vector<double> weights;
    for (int i = 0; i < static_cast<int>(r.size()); ++i) {
        if (r[static_cast<std::size_t>(i)] >= min_w) {
            const auto [a, b] = combo_cards(i);
            indices.push_back(a);
            indices.push_back(b);
            weights.push_back(r[static_cast<std::size_t>(i)]);
        }
    }
    const double s = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (s > 0.0) {
        for (double& w : weights) {
            w /= s;
        }
    }
    Napi::Object o = Napi::Object::New(env);
    Napi::Array ia = Napi::Array::New(env, static_cast<uint32_t>(indices.size()));
    for (std::size_t i = 0; i < indices.size(); ++i) {
        ia[static_cast<uint32_t>(i)] = indices[i];
    }
    o.Set("indices", ia);
    o.Set("weights", poker_bind::write_f64_vector(env, weights, poker_bind::F64ReturnFormat::Float64));
    return o;
}

Napi::Value MergeSparseRanges(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 4 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "mergeSparseRanges(a, b, weightA, weightB)" : err);
    }
    const double wa = get_number(info, 2, 0.5);
    const double wb = get_number(info, 3, 0.5);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = std::max(0.0, wa * a[i] + wb * b[i]);
    }
    return write_dense(env, normalized(a));
}

Napi::Value IntersectSparseRanges(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "intersectSparseRanges(a, b)" : err);
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = std::min(a[i], b[i]);
    }
    return write_dense(env, normalized(a));
}

Napi::Value SubtractSparseRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "subtractSparseRange(base, remove)" : err);
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = std::max(0.0, a[i] - b[i]);
    }
    return write_dense(env, normalized(a));
}

Napi::Value RangeComboCount(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeComboCount(range, minWeight?)" : err);
    }
    const double min_w = get_number(info, 1, 0.0);
    return Napi::Number::New(env, std::count_if(r.begin(), r.end(), [&](double w) { return w > min_w; }));
}

Napi::Value RangeShannonEntropy(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeShannonEntropy(range)" : err);
    }
    r = normalized(r);
    double h = 0.0;
    for (double p : r) {
        if (p > 0.0) {
            h -= p * std::log(p);
        }
    }
    return Napi::Number::New(env, h);
}

Napi::Value RangeGiniCoefficient(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeGiniCoefficient(range)" : err);
    }
    std::sort(r.begin(), r.end());
    const double s = sum_positive(r);
    if (s <= 0.0) {
        return Napi::Number::New(env, 0);
    }
    double weighted = 0.0;
    for (std::size_t i = 0; i < r.size(); ++i) {
        weighted += (static_cast<double>(i) + 1.0) * r[i];
    }
    return Napi::Number::New(env, (2.0 * weighted) / (r.size() * s) - (static_cast<double>(r.size()) + 1.0) / r.size());
}

Napi::Value RangeCoverageFraction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeCoverageFraction(range)" : err);
    }
    return Napi::Number::New(env, std::count_if(r.begin(), r.end(), [](double w) { return w > 0.0; }) / 1326.0);
}

Napi::Value RangeWeightTopKMass(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeWeightTopKMass(range, k)" : err);
    }
    r = normalized(r);
    std::sort(r.begin(), r.end(), std::greater<>());
    const int k = std::max(0, info[1].As<Napi::Number>().Int32Value());
    return Napi::Number::New(env, std::accumulate(r.begin(), r.begin() + std::min<int>(k, r.size()), 0.0));
}

Napi::Value RangeDistanceL1(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeDistanceL1(a, b)" : err);
    }
    a = normalized(a);
    b = normalized(b);
    double d = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        d += std::abs(a[i] - b[i]);
    }
    return Napi::Number::New(env, d);
}

Napi::Value RangeDistanceL2(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeDistanceL2(a, b)" : err);
    }
    a = normalized(a);
    b = normalized(b);
    double d = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        d += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return Napi::Number::New(env, std::sqrt(d));
}

Napi::Value RangeDistanceJensenShannon(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeDistanceJensenShannon(a, b)" : err);
    }
    a = normalized(a);
    b = normalized(b);
    double js = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double m = 0.5 * (a[i] + b[i]);
        if (a[i] > 0.0 && m > 0.0) {
            js += 0.5 * a[i] * std::log(a[i] / m);
        }
        if (b[i] > 0.0 && m > 0.0) {
            js += 0.5 * b[i] * std::log(b[i] / m);
        }
    }
    return Napi::Number::New(env, std::sqrt(std::max(0.0, js)));
}

Napi::Value RangeCosineSimilarity(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeCosineSimilarity(a, b)" : err);
    }
    double dot = 0.0;
    double aa = 0.0;
    double bb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    return Napi::Number::New(env, aa > 0.0 && bb > 0.0 ? dot / std::sqrt(aa * bb) : 0.0);
}

Napi::Value RangeTopCombos(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeTopCombos(range, k)" : err);
    }
    std::vector<std::pair<int, double>> pairs;
    for (int i = 0; i < static_cast<int>(r.size()); ++i) {
        if (r[static_cast<std::size_t>(i)] > 0.0) {
            pairs.push_back({i, r[static_cast<std::size_t>(i)]});
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
    const int k = std::min<int>(std::max(0, info[1].As<Napi::Number>().Int32Value()), pairs.size());
    Napi::Array arr = Napi::Array::New(env, k);
    for (int i = 0; i < k; ++i) {
        const auto [a, b] = combo_cards(pairs[static_cast<std::size_t>(i)].first);
        Napi::Object o = Napi::Object::New(env);
        o.Set("comboIndex", pairs[static_cast<std::size_t>(i)].first);
        o.Set("cardA", a);
        o.Set("cardB", b);
        o.Set("notation", combo_notation(a, b));
        o.Set("weight", pairs[static_cast<std::size_t>(i)].second);
        arr[i] = o;
    }
    return arr;
}

Napi::Value RangeBucketWeightsByHandClass(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeBucketWeightsByHandClass(range)" : err);
    }
    Napi::Object o = Napi::Object::New(env);
    double pairs = 0, suited_broadway = 0, offsuit_broadway = 0, suited_connectors = 0, suited_aces = 0, other = 0;
    for (int i = 0; i < static_cast<int>(r.size()); ++i) {
        const double w = r[static_cast<std::size_t>(i)];
        if (w <= 0.0) {
            continue;
        }
        const auto [a, b] = combo_cards(i);
        const int ra = a / 4;
        const int rb = b / 4;
        const bool suited = a % 4 == b % 4;
        if (ra == rb) {
            pairs += w;
        } else if (ra >= 9 && rb >= 9) {
            suited ? suited_broadway += w : offsuit_broadway += w;
        } else if (suited && (ra == 12 || rb == 12)) {
            suited_aces += w;
        } else if (suited && std::abs(ra - rb) == 1) {
            suited_connectors += w;
        } else {
            other += w;
        }
    }
    o.Set("pairs", pairs);
    o.Set("suitedBroadways", suited_broadway);
    o.Set("offsuitBroadways", offsuit_broadway);
    o.Set("suitedConnectors", suited_connectors);
    o.Set("suitedAces", suited_aces);
    o.Set("other", other);
    return o;
}

Napi::Value RangeBucketWeightsByNotation(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeBucketWeightsByNotation(range)" : err);
    }
    std::vector<std::pair<std::string, double>> buckets;
    for (int i = 0; i < static_cast<int>(r.size()); ++i) {
        const double w = r[static_cast<std::size_t>(i)];
        if (w <= 0.0) {
            continue;
        }
        const auto [a, b] = combo_cards(i);
        const std::string n = combo_notation(a, b);
        auto it = std::find_if(buckets.begin(), buckets.end(), [&](const auto& p) { return p.first == n; });
        if (it == buckets.end()) {
            buckets.push_back({n, w});
        } else {
            it->second += w;
        }
    }
    std::sort(buckets.begin(), buckets.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    Napi::Array arr = Napi::Array::New(env, static_cast<uint32_t>(buckets.size()));
    for (std::size_t i = 0; i < buckets.size(); ++i) {
        Napi::Object o = Napi::Object::New(env);
        o.Set("notation", buckets[i].first);
        o.Set("weight", buckets[i].second);
        arr[static_cast<uint32_t>(i)] = o;
    }
    return arr;
}

Napi::Value RangeFromNotationWeights(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsArray()) {
        POKER_FAIL_TYPE(env, "rangeFromNotationWeights(entries)");
    }
    std::vector<double> out(kCombos, 0.0);
    const Napi::Array entries = info[0].As<Napi::Array>();
    for (uint32_t i = 0; i < entries.Length(); ++i) {
        if (!entries.Get(i).IsObject()) {
            continue;
        }
        const Napi::Object e = entries.Get(i).As<Napi::Object>();
        if (!e.Has("notation") || !e.Has("weight")) {
            continue;
        }
        std::vector<int> idxs;
        if (!notation_to_combos(e.Get("notation").As<Napi::String>().Utf8Value(), idxs)) {
            continue;
        }
        const double w = e.Get("weight").As<Napi::Number>().DoubleValue();
        for (int idx : idxs) {
            if (idx >= 0) {
                out[static_cast<std::size_t>(idx)] += w / std::max<std::size_t>(1, idxs.size());
            }
        }
    }
    return write_dense(env, normalized(out));
}

Napi::Value RangeBlockerPressureByCard(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeBlockerPressureByCard(range, deadCards?)" : err);
    }
    std::vector<double> out(52, 0.0);
    for (int i = 0; i < static_cast<int>(r.size()); ++i) {
        const auto [a, b] = combo_cards(i);
        out[static_cast<std::size_t>(a)] += r[static_cast<std::size_t>(i)];
        out[static_cast<std::size_t>(b)] += r[static_cast<std::size_t>(i)];
    }
    return write_dense(env, out);
}

Napi::Value RangeRemovalSensitivityVsHero(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) {
        POKER_FAIL_TYPE(env, "rangeRemovalSensitivityVsHero(heroHoleCards, boardCards, range)");
    }
    std::string err;
    const auto hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> r;
    if (!read_range_dense(info[2], r, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double base = equity_vs_range_safe(hero, board, r);
    std::vector<double> out(52, 0.0);
    const std::uint64_t dead = dead_mask_from_groups(hero, board);
    for (int c = 0; c < 52; ++c) {
        if ((dead & (std::uint64_t{1} << c)) != 0) {
            continue;
        }
        std::vector<double> blocked = r;
        for (int i = 0; i < static_cast<int>(blocked.size()); ++i) {
            const auto [a, b] = combo_cards(i);
            if (a == c || b == c) {
                blocked[static_cast<std::size_t>(i)] = 0.0;
            }
        }
        out[static_cast<std::size_t>(c)] = equity_vs_range_safe(hero, board, blocked) - base;
    }
    return write_dense(env, out);
}

Napi::Value BlockerMatrixByCard(const Napi::CallbackInfo& info) {
    return RangeBlockerPressureByCard(info);
}

Napi::Value GeometricStreetSizingPlan(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "geometricStreetSizingPlan(pot, effectiveStack, streetsRemaining)");
    const double pot = std::max(1e-9, get_number(info, 0));
    const double stack = std::max(0.0, get_number(info, 1));
    const int n = std::max(0, info[2].As<Napi::Number>().Int32Value());
    std::vector<double> bets;
    if (n > 0) {
        const double f = (std::pow((pot + 2.0 * stack) / pot, 1.0 / n) - 1.0) / 2.0;
        double p = pot;
        for (int i = 0; i < n; ++i) {
            const double b = p * f;
            bets.push_back(b);
            p += 2.0 * b;
        }
    }
    return poker_bind::write_f64_vector(env, bets, poker_bind::F64ReturnFormat::Float64);
}

Napi::Value ThinValueMargin(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "thinValueMargin(heroEquityWhenCalled, pot, betSize)");
    return Napi::Number::New(env, get_number(info, 0) * (get_number(info, 1) + 2.0 * get_number(info, 2)) - get_number(info, 2));
}

Napi::Value BetSizingIndifferencePoint(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "betSizingIndifferencePoint(pot, foldFrequency, equityWhenCalled)");
    const double pot = get_number(info, 0);
    const double fe = clamp01(get_number(info, 1));
    const double eq = clamp01(get_number(info, 2));
    const double denom = std::max(1e-9, 1.0 - fe - 2.0 * eq * (1.0 - fe));
    return Napi::Number::New(env, std::max(0.0, ((fe + eq * (1.0 - fe)) * pot) / denom));
}

Napi::Value OpponentFoldToCbetPosterior(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "opponentFoldToCbetPosterior(priorAlpha, priorBeta, folds, continues)");
    const auto r = poker::beta_binomial_fold_update(get_number(info, 0), get_number(info, 1), info[2].As<Napi::Number>().Int32Value(), info[3].As<Napi::Number>().Int32Value());
    Napi::Object o = Napi::Object::New(env);
    o.Set("alpha", r.alpha);
    o.Set("beta", r.beta);
    o.Set("posteriorMean", r.posterior_mean);
    return o;
}

Napi::Value OpponentAggressionFactor(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "opponentAggressionFactor(bets, raises, calls)");
    return Napi::Number::New(env, (get_number(info, 0) + get_number(info, 1)) / std::max(1.0, get_number(info, 2)));
}

Napi::Value OpponentRangeElasticityFromSizing(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 2) POKER_FAIL_TYPE(env, "opponentRangeElasticityFromSizing(sizes, continueRates)");
    std::string err;
    std::vector<double> sizes, rates;
    if (!read_f64_vector(info[0], "sizes", sizes, &err) || !read_f64_vector(info[1], "continueRates", rates, &err)) POKER_FAIL_TYPE(env, err);
    if (sizes.size() < 2 || rates.size() < 2) return Napi::Number::New(env, 0);
    const double ds = sizes.back() - sizes.front();
    return Napi::Number::New(env, ds != 0.0 ? (rates.back() - rates.front()) / ds : 0.0);
}

Napi::Value VillainPolarizedRangeScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) POKER_FAIL_TYPE(env, err.empty() ? "villainPolarizedRangeScore(range, board)" : err);
    return Napi::Number::New(env, clamp01(RangeGiniCoefficient(info).As<Napi::Number>().DoubleValue()));
}

Napi::Value LegalActionSummary(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker::PokerGameState state;
    std::string err;
    if (info.Length() < 1 || !poker_bind::parse_state_input(info[0], state, &err)) POKER_FAIL_TYPE(env, err.empty() ? "legalActionSummary(state)" : err);
    return legal_summary_from_state(env, state);
}

Napi::Value ActionMaskFromState(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    Napi::Object o = LegalActionSummary(info).As<Napi::Object>();
    int mask = 0;
    if (o.Get("canFold").As<Napi::Boolean>().Value()) mask |= 1;
    if (o.Get("canCheck").As<Napi::Boolean>().Value()) mask |= 2;
    if (o.Get("canCall").As<Napi::Boolean>().Value()) mask |= 4;
    if (o.Get("canRaise").As<Napi::Boolean>().Value()) mask |= 8;
    return Napi::Number::New(env, mask);
}

Napi::Value NormalizeBotConfig(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker::BotConfig cfg{};
    if (info.Length() >= 1 && info[0].IsObject()) {
        cfg = poker_bind::parse_bot_config(info[0].As<Napi::Object>());
    }
    cfg.aggression_threshold = static_cast<float>(clamp01(cfg.aggression_threshold));
    cfg.risk_tolerance = static_cast<float>(std::max(0.0f, cfg.risk_tolerance));
    cfg.monte_carlo_simulations = std::max(0, cfg.monte_carlo_simulations);
    cfg.monte_carlo_villains = std::max(1, cfg.monte_carlo_villains);
    cfg.raise_pot_fraction = static_cast<float>(std::max(0.0f, cfg.raise_pot_fraction));
    return config_to_js(env, cfg);
}

Napi::Value ValidatePokerState(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker::PokerGameState state;
    std::string err;
    const bool ok = info.Length() >= 1 && poker_bind::parse_state_input(info[0], state, &err);
    Napi::Object o = Napi::Object::New(env);
    o.Set("valid", ok);
    Napi::Array errors = Napi::Array::New(env, ok ? 0 : 1);
    if (!ok) errors[0u] = err.empty() ? "invalid state" : err;
    o.Set("errors", errors);
    return o;
}

Napi::Value StateToFeatureVector(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker::PokerGameState state;
    std::string err;
    if (info.Length() < 1 || !poker_bind::parse_state_input(info[0], state, &err)) POKER_FAIL_TYPE(env, err.empty() ? "stateToFeatureVector(state)" : err);
    std::vector<double> v{
        static_cast<double>(state.players.size()), static_cast<double>(state.community_cards.size()),
        static_cast<double>(state.pot), static_cast<double>(state.current_bet),
        static_cast<double>(state.small_blind), static_cast<double>(state.big_blind),
        static_cast<double>(state.acting_index), static_cast<double>(state.last_raise_increment),
    };
    return poker_bind::write_f64_vector(env, v, poker_bind::F64ReturnFormat::Float64);
}

Napi::Value DecideActionWithDiagnostics(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker_bind::DecideActionParsed parsed;
    std::string err;
    if (!poker_bind::parse_decide_action_inputs(info, parsed, &err)) POKER_FAIL_TYPE(env, err);
    const poker::OpponentModel* opp = parsed.opponent ? &*parsed.opponent : nullptr;
    const auto d = poker::decide_action(parsed.state, parsed.hero_hole, parsed.cfg, opp, parsed.hero_seat);
    Napi::Object o = Napi::Object::New(env);
    o.Set("decision", decision_to_js(env, d));
    o.Set("legalActions", legal_summary_from_state(env, parsed.state));
    o.Set("reason", "rule-based equity and pot-odds decision");
    return o;
}

Napi::Value RunBotPolicyBatch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsArray()) POKER_FAIL_TYPE(env, "runBotPolicyBatch(states, config, opponentModels?)");
    const Napi::Array states = info[0].As<Napi::Array>();
    Napi::Array out = Napi::Array::New(env, states.Length());
    if (!info[1].IsObject()) {
        POKER_FAIL_TYPE(env, "config must be an object");
    }
    const poker::BotConfig cfg = poker_bind::parse_bot_config(info[1].As<Napi::Object>());
    for (uint32_t i = 0; i < states.Length(); ++i) {
        poker::PokerGameState state;
        std::string err;
        if (!poker_bind::parse_state_input(states.Get(i), state, &err)) {
            Napi::Object row = Napi::Object::New(env);
            row.Set("error", err.empty() ? "invalid state" : err);
            out[i] = row;
            continue;
        }
        std::vector<poker::Card> hero;
        poker_bind::resolve_hero_hole(state, -1, hero);
        const auto decision = poker::decide_action(state, hero, cfg, nullptr, -1);
        Napi::Object row = Napi::Object::New(env);
        row.Set("decision", decision_to_js(env, decision));
        row.Set("legalActions", legal_summary_from_state(env, state));
        row.Set("reason", "rule-based equity and pot-odds decision");
        out[i] = row;
    }
    return out;
}
