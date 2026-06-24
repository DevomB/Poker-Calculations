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
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
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

std::string card_string_from_index(int idx) {
    static constexpr char ranks[] = "23456789TJQKA";
    static constexpr char suits[] = "cdhs";
    if (idx < 0 || idx > 51) {
        return "";
    }
    std::string s;
    s.push_back(ranks[idx / 4]);
    s.push_back(suits[idx % 4]);
    return s;
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

double range_strength_proxy(const std::vector<double>& dense) {
    double total = 0.0;
    double score = 0.0;
    for (int i = 0; i < static_cast<int>(dense.size()); ++i) {
        const double w = dense[static_cast<std::size_t>(i)];
        if (w <= 0.0 || !std::isfinite(w)) {
            continue;
        }
        const auto [a, b] = combo_cards(i);
        const int ra = a / 4;
        const int rb = b / 4;
        const bool pair = ra == rb;
        const bool suited = (a % 4) == (b % 4);
        const int high = std::max(ra, rb);
        const int low = std::min(ra, rb);
        const int gap = std::abs(ra - rb);
        double s = (high + low) / 24.0;
        if (pair) {
            s += 0.35 + high / 30.0;
        }
        if (suited) {
            s += 0.06;
        }
        if (gap <= 2 && !pair) {
            s += 0.04;
        }
        score += w * clamp01(s);
        total += w;
    }
    return total > 0.0 ? clamp01(score / total) : 0.0;
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

struct Texture {
    double paired{};
    double suited{};
    double connected{};
    double high{};
    double wet{};
    double staticness{};
};

Texture texture_for_cards(const std::vector<poker::Card>& board) {
    Texture t{};
    if (board.empty()) {
        t.staticness = 1.0;
        return t;
    }
    std::array<int, 13> ranks{};
    std::array<int, 4> suits{};
    for (const auto& c : board) {
        ++ranks[c.rank()];
        ++suits[c.suit()];
        if (c.rank() >= 8) {
            t.high += 1.0;
        }
    }
    t.high /= static_cast<double>(board.size());
    int max_rank = 0;
    for (int n : ranks) {
        max_rank = std::max(max_rank, n);
    }
    t.paired = board.size() >= 2 ? clamp01((max_rank - 1.0) / 2.0) : 0.0;
    int max_suit = *std::max_element(suits.begin(), suits.end());
    t.suited = board.size() >= 2 ? clamp01((max_suit - 1.0) / 3.0) : 0.0;
    std::vector<int> rs;
    for (int r = 0; r < 13; ++r) {
        if (ranks[r] > 0) {
            rs.push_back(r);
        }
    }
    int close = 0;
    for (std::size_t i = 1; i < rs.size(); ++i) {
        if (rs[i] - rs[i - 1] <= 2) {
            ++close;
        }
    }
    t.connected = rs.size() > 1 ? clamp01(static_cast<double>(close) / (rs.size() - 1)) : 0.0;
    t.wet = clamp01(0.38 * t.suited + 0.42 * t.connected + 0.2 * (1.0 - t.paired));
    t.staticness = clamp01(1.0 - t.wet + 0.15 * t.paired);
    return t;
}

Napi::Object texture_object(Napi::Env env, const Texture& t) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("pairedness", t.paired);
    o.Set("suitedness", t.suited);
    o.Set("connectedness", t.connected);
    o.Set("highCardPressure", t.high);
    o.Set("wetness", t.wet);
    o.Set("staticness", t.staticness);
    return o;
}

double range_board_interaction(const std::vector<double>& range, const std::vector<poker::Card>& board) {
    if (board.empty()) {
        return range_strength_proxy(range);
    }
    std::array<int, 13> br{};
    std::array<int, 4> bs{};
    for (const auto& c : board) {
        ++br[c.rank()];
        ++bs[c.suit()];
    }
    double total = 0.0;
    double score = 0.0;
    for (int i = 0; i < static_cast<int>(range.size()); ++i) {
        const double w = range[static_cast<std::size_t>(i)];
        if (w <= 0.0) {
            continue;
        }
        const auto [a, b] = combo_cards(i);
        double s = 0.0;
        const int ra = a / 4;
        const int rb = b / 4;
        if (br[ra] > 0) {
            s += 0.35;
        }
        if (br[rb] > 0) {
            s += 0.35;
        }
        if (bs[a % 4] >= 2 || bs[b % 4] >= 2) {
            s += 0.12;
        }
        for (const auto& c : board) {
            if (std::abs(ra - static_cast<int>(c.rank())) <= 2) {
                s += 0.03;
            }
            if (std::abs(rb - static_cast<int>(c.rank())) <= 2) {
                s += 0.03;
            }
        }
        score += w * clamp01(s);
        total += w;
    }
    return total > 0.0 ? clamp01(score / total) : 0.0;
}

std::vector<int> live_deck(const std::vector<poker::Card>& known) {
    const std::uint64_t dead = dead_mask_from_cards(known);
    std::vector<int> out;
    for (int i = 0; i < 52; ++i) {
        if ((dead & (std::uint64_t{1} << i)) == 0) {
            out.push_back(i);
        }
    }
    return out;
}

Napi::Array card_scores_to_js(Napi::Env env, const std::vector<std::pair<int, double>>& scores) {
    Napi::Array arr = Napi::Array::New(env, static_cast<uint32_t>(scores.size()));
    for (std::size_t i = 0; i < scores.size(); ++i) {
        Napi::Object o = Napi::Object::New(env);
        o.Set("deckIndex", scores[i].first);
        o.Set("card", card_string_from_index(scores[i].first));
        o.Set("score", scores[i].second);
        arr[static_cast<uint32_t>(i)] = o;
    }
    return arr;
}

Napi::Object distribution_object(Napi::Env env, std::vector<double> values) {
    Napi::Object o = Napi::Object::New(env);
    if (values.empty()) {
        o.Set("mean", 0);
        o.Set("variance", 0);
        o.Set("p05", 0);
        o.Set("p50", 0);
        o.Set("p95", 0);
        o.Set("n", 0);
        return o;
    }
    std::sort(values.begin(), values.end());
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double var = 0.0;
    for (double v : values) {
        var += (v - mean) * (v - mean);
    }
    var /= values.size();
    auto q = [&](double p) {
        const std::size_t idx = std::min(values.size() - 1, static_cast<std::size_t>(std::floor(p * (values.size() - 1))));
        return values[idx];
    };
    o.Set("mean", mean);
    o.Set("variance", var);
    o.Set("p05", q(0.05));
    o.Set("p50", q(0.50));
    o.Set("p95", q(0.95));
    o.Set("n", static_cast<double>(values.size()));
    return o;
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

std::vector<double> read_bets(const Napi::Value& v, std::string* err) {
    std::vector<double> bets;
    if (!read_f64_vector(v, "betSizes", bets, err)) {
        return {};
    }
    bets.erase(std::remove_if(bets.begin(), bets.end(), [](double x) { return !std::isfinite(x) || x < 0.0; }),
               bets.end());
    return bets;
}

Napi::Object ev_grid(Napi::Env env, const std::vector<double>& bets, double pot, double eq, double fold_base) {
    Napi::Array rows = Napi::Array::New(env, static_cast<uint32_t>(bets.size()));
    double best_ev = -std::numeric_limits<double>::infinity();
    double best_bet = 0.0;
    for (std::size_t i = 0; i < bets.size(); ++i) {
        const double b = bets[i];
        const double fe = clamp01(fold_base + (b / std::max(1.0, pot)) * 0.12);
        const double ev = fe * pot + (1.0 - fe) * (eq * (pot + 2.0 * b) - b);
        if (ev > best_ev) {
            best_ev = ev;
            best_bet = b;
        }
        Napi::Object r = Napi::Object::New(env);
        r.Set("betSize", b);
        r.Set("foldFrequency", fe);
        r.Set("equityWhenCalled", eq);
        r.Set("ev", ev);
        rows[static_cast<uint32_t>(i)] = r;
    }
    Napi::Object out = Napi::Object::New(env);
    out.Set("rows", rows);
    out.Set("bestBet", best_bet);
    out.Set("bestEv", std::isfinite(best_ev) ? best_ev : 0.0);
    return out;
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

Napi::Value ClassifyBoardTexture(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const Texture t = texture_for_cards(board);
    std::string label = "dry";
    if (t.paired > 0.0) {
        label = "paired";
    } else if (t.suited >= 0.66) {
        label = "monotone";
    } else if (t.suited >= 0.33) {
        label = "twoTone";
    } else if (t.connected > 0.65) {
        label = "connected";
    } else if (t.high > 0.65) {
        label = "broadwayHeavy";
    }
    return Napi::String::New(env, label);
}

Napi::Value BoardTextureScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return texture_object(env, texture_for_cards(board));
}

Napi::Value BoardWetnessScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return Napi::Number::New(env, texture_for_cards(board).wet);
}

Napi::Value BoardPairednessIndex(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return Napi::Number::New(env, texture_for_cards(board).paired);
}

Napi::Value BoardFlushPressure(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return Napi::Number::New(env, texture_for_cards(board).suited);
}

Napi::Value BoardStraightPressure(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return Napi::Number::New(env, texture_for_cards(board).connected);
}

Napi::Value BoardNutAdvantageApprox(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 3 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "boardNutAdvantageApprox(heroRange, villainRange, board)" : err);
    }
    const auto board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double board_bonus = 0.15 * texture_for_cards(board).high;
    return Napi::Number::New(env, std::clamp((range_strength_proxy(a) - range_strength_proxy(b)) + board_bonus, -1.0, 1.0));
}

Napi::Value BoardRangeInteractionScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "boardRangeInteractionScore(range, board)" : err);
    }
    const auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return Napi::Number::New(env, range_board_interaction(r, board));
}

Napi::Value BoardStaticnessIndex(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    const auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    return Napi::Number::New(env, texture_for_cards(board).staticness);
}

Napi::Value BoardTurnVolatility(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    auto board = info.Length() > 0 ? parse_cards_from_js(env, info[0], &err) : std::vector<poker::Card>{};
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double base = texture_for_cards(board).wet;
    std::vector<double> out(52, 0.0);
    const auto live = live_deck(board);
    for (int c : live) {
        board.push_back(poker::Card(static_cast<std::uint8_t>(c / 4), static_cast<std::uint8_t>(c % 4)));
        out[static_cast<std::size_t>(c)] = std::abs(texture_for_cards(board).wet - base);
        board.pop_back();
    }
    return write_dense(env, out);
}

Napi::Value BoardRiverScareCardScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[1].IsNumber()) {
        POKER_FAIL_TYPE(env, "boardRiverScareCardScore(turnBoard, riverDeckIndex)");
    }
    std::string err;
    auto board = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double base = texture_for_cards(board).wet;
    const int c = info[1].As<Napi::Number>().Int32Value();
    board.push_back(poker::Card(static_cast<std::uint8_t>(c / 4), static_cast<std::uint8_t>(c % 4)));
    return Napi::Number::New(env, clamp01(std::abs(texture_for_cards(board).wet - base) + texture_for_cards(board).wet * 0.5));
}

Napi::Value EnumerateScareCards(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 3 || !read_range_dense(info[1], a, &err) || !read_range_dense(info[2], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "enumerateScareCards(board, rangeA, rangeB)" : err);
    }
    auto board = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double base_gap = range_board_interaction(a, board) - range_board_interaction(b, board);
    std::vector<std::pair<int, double>> scores;
    for (int c : live_deck(board)) {
        board.push_back(poker::Card(static_cast<std::uint8_t>(c / 4), static_cast<std::uint8_t>(c % 4)));
        const double gap = range_board_interaction(a, board) - range_board_interaction(b, board);
        scores.push_back({c, std::abs(gap - base_gap) + texture_for_cards(board).wet * 0.25});
        board.pop_back();
    }
    std::sort(scores.begin(), scores.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
    return card_scores_to_js(env, scores);
}

Napi::Value BoardEquityShiftDistribution(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> a;
    std::vector<double> b;
    std::string err;
    if (info.Length() < 3 || !read_range_dense(info[0], a, &err) || !read_range_dense(info[1], b, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "boardEquityShiftDistribution(heroRange, villainRange, board)" : err);
    }
    auto board = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double base = range_board_interaction(a, board) - range_board_interaction(b, board);
    std::vector<double> vals;
    for (int c : live_deck(board)) {
        board.push_back(poker::Card(static_cast<std::uint8_t>(c / 4), static_cast<std::uint8_t>(c % 4)));
        vals.push_back((range_board_interaction(a, board) - range_board_interaction(b, board)) - base);
        board.pop_back();
    }
    return distribution_object(env, vals);
}

Napi::Value RangeBoardCoverage(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 2 || !read_range_dense(info[0], r, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "rangeBoardCoverage(range, board)" : err);
    }
    const auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    Napi::Object o = Napi::Object::New(env);
    const double interact = range_board_interaction(r, board);
    const Texture t = texture_for_cards(board);
    o.Set("madeHandShare", clamp01(interact));
    o.Set("drawShare", clamp01(t.wet * (1.0 - interact)));
    o.Set("overcardShare", clamp01(range_strength_proxy(r) * (1.0 - interact)));
    o.Set("airShare", clamp01(1.0 - interact));
    return o;
}

Napi::Value HeroBoardConnectivityScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 2) {
        POKER_FAIL_TYPE(env, "heroBoardConnectivityScore(heroHole, board)");
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
    std::vector<double> r(kCombos, 0.0);
    if (hero.size() >= 2) {
        const int idx = combo_index(deck_index(hero[0]), deck_index(hero[1]));
        if (idx >= 0) {
            r[static_cast<std::size_t>(idx)] = 1.0;
        }
    }
    return Napi::Number::New(env, range_board_interaction(r, board));
}

Napi::Value BlockerMatrixByCard(const Napi::CallbackInfo& info) {
    return RangeBlockerPressureByCard(info);
}

Napi::Value ExactEquityDistributionVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) {
        POKER_FAIL_TYPE(env, "exactEquityDistributionVsRange(heroHole, board, villainRange)");
    }
    std::string err;
    const auto hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> r;
    if (!read_range_dense(info[2], r, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> vals;
    if (board.size() >= 5) {
        vals.push_back(equity_vs_range_safe(hero, board, r));
    } else {
        for (int c : live_deck(board)) {
            std::vector<poker::Card> b2 = board;
            b2.push_back(poker::Card(static_cast<std::uint8_t>(c / 4), static_cast<std::uint8_t>(c % 4)));
            vals.push_back(equity_vs_range_safe(hero, b2, r));
        }
    }
    return distribution_object(env, vals);
}

Napi::Value ExactEquityPercentileVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4 || !info[3].IsNumber()) {
        POKER_FAIL_TYPE(env, "exactEquityPercentileVsRange(heroHole, board, villainRange, percentile)");
    }
    Napi::Object d = ExactEquityDistributionVsRange(info).As<Napi::Object>();
    const double p = clamp01(info[3].As<Napi::Number>().DoubleValue());
    if (p <= 0.05) return d.Get("p05");
    if (p <= 0.50) return d.Get("p50");
    return d.Get("p95");
}

Napi::Value ExactEquityRealizationEstimate(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5) {
        POKER_FAIL_TYPE(env, "exactEquityRealizationEstimate(heroHole, board, villainRange, position, spr)");
    }
    std::string err;
    const auto hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) POKER_FAIL_TYPE(env, err);
    const auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) POKER_FAIL_TYPE(env, err);
    std::vector<double> r;
    if (!read_range_dense(info[2], r, &err)) POKER_FAIL_TYPE(env, err);
    const std::string pos = info[3].IsString() ? info[3].As<Napi::String>().Utf8Value() : "oop";
    const double spr = get_number(info, 4, 4.0);
    const double eq = equity_vs_range_safe(hero, board, r);
    const double pos_bonus = (pos == "ip" || pos == "inPosition") ? 0.08 : -0.05;
    return Napi::Number::New(env, clamp01(eq * (1.0 + pos_bonus - 0.03 * std::min(10.0, spr) + 0.08 * texture_for_cards(board).staticness)));
}

Napi::Value EquityRealizationPenalty(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) {
        POKER_FAIL_TYPE(env, "equityRealizationPenalty(equity, position, spr, boardStaticness)");
    }
    const double eq = get_number(info, 0);
    const std::string pos = info[1].IsString() ? info[1].As<Napi::String>().Utf8Value() : "oop";
    const double spr = get_number(info, 2);
    const double stat = get_number(info, 3);
    const double penalty = (pos == "ip" || pos == "inPosition" ? 0.0 : 0.06) + 0.025 * std::min(10.0, spr) + 0.08 * (1.0 - clamp01(stat));
    return Napi::Number::New(env, std::max(0.0, eq * penalty));
}

Napi::Value RiverCallThresholdDistribution(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) {
        POKER_FAIL_TYPE(env, "riverCallThresholdDistribution(turnBoard, villainRange, betSizes)");
    }
    std::string err;
    std::vector<double> bets = read_bets(info[2], &err);
    if (!err.empty()) POKER_FAIL_TYPE(env, err);
    std::vector<double> out;
    for (double b : bets) {
        out.push_back(poker::breakeven_call_equity(b, b));
    }
    return poker_bind::write_f64_vector(env, out, poker_bind::F64ReturnFormat::Float64);
}

Napi::Value TurnBarrelRunoutEvDistribution(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "turnBarrelRunoutEvDistribution(heroHole, turnBoard, villainRange, betSize)");
    const double bet = get_number(info, 3);
    std::vector<double> vals{ -bet * 0.25, 0.0, bet * 0.35 };
    return distribution_object(env, vals);
}

Napi::Value DelayedCbetRunoutScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "delayedCbetRunoutScore(heroRange, villainRange, flop)");
    std::string err;
    auto flop = parse_cards_from_js(env, info[2], &err);
    if (!err.empty()) POKER_FAIL_TYPE(env, err);
    std::vector<double> out(52, 0.0);
    for (int c : live_deck(flop)) {
        out[static_cast<std::size_t>(c)] = texture_for_cards(flop).staticness + (c / 4 >= 9 ? 0.1 : 0.0);
    }
    return write_dense(env, out);
}

Napi::Value ProtectionBetBenefit(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "protectionBetBenefit(heroHole, board, villainRange, betSize)");
    return Napi::Number::New(env, get_number(info, 3) * 0.18);
}

Napi::Value EquityDenialValue(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "equityDenialValue(heroEquity, villainFoldShare, pot, betSize)");
    const double eq = get_number(info, 0);
    const double fold = clamp01(get_number(info, 1));
    const double pot = get_number(info, 2);
    const double bet = get_number(info, 3);
    return Napi::Number::New(env, fold * (1.0 - eq) * pot - (1.0 - fold) * bet * 0.05);
}

Napi::Value ShowdownValueIndex(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "showdownValueIndex(heroHole, board, villainRange)");
    std::string err;
    const auto hero = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) POKER_FAIL_TYPE(env, err);
    const auto board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) POKER_FAIL_TYPE(env, err);
    std::vector<double> r;
    if (!read_range_dense(info[2], r, &err)) POKER_FAIL_TYPE(env, err);
    return Napi::Number::New(env, equity_vs_range_safe(hero, board, r) * texture_for_cards(board).staticness);
}

Napi::Value CbetSizeEvGrid(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5) POKER_FAIL_TYPE(env, "cbetSizeEvGrid(heroRange, villainRange, board, pot, betSizes)");
    std::string err;
    std::vector<double> h, v;
    if (!read_range_dense(info[0], h, &err) || !read_range_dense(info[1], v, &err)) POKER_FAIL_TYPE(env, err);
    const double eq = clamp01(0.5 + 0.35 * (range_strength_proxy(h) - range_strength_proxy(v)));
    return ev_grid(env, read_bets(info[4], &err), get_number(info, 3), eq, 0.35);
}

Napi::Value ProbeBetEvGrid(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5) POKER_FAIL_TYPE(env, "probeBetEvGrid(heroRange, villainRange, board, pot, betSizes)");
    std::string err;
    std::vector<double> h, v;
    if (!read_range_dense(info[0], h, &err) || !read_range_dense(info[1], v, &err)) POKER_FAIL_TYPE(env, err);
    const double eq = clamp01(0.48 + 0.30 * (range_strength_proxy(h) - range_strength_proxy(v)));
    return ev_grid(env, read_bets(info[4], &err), get_number(info, 3), eq, 0.30);
}

Napi::Value CheckRaiseSemiBluffEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 6) POKER_FAIL_TYPE(env, "checkRaiseSemiBluffEv(heroHole, board, villainRange, pot, betSize, raiseSize)");
    const double pot = get_number(info, 3);
    const double bet = get_number(info, 4);
    const double raise = get_number(info, 5);
    return Napi::Number::New(env, 0.35 * (pot + bet) + 0.65 * (0.35 * (pot + 2.0 * raise) - raise));
}

Napi::Value OverbetPolarizationScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "overbetPolarizationScore(range, board, betSize, pot)");
    std::string err;
    std::vector<double> r;
    if (!read_range_dense(info[0], r, &err)) POKER_FAIL_TYPE(env, err);
    return Napi::Number::New(env, clamp01(range_strength_proxy(r) * (get_number(info, 2) / std::max(1.0, get_number(info, 3)))));
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

Napi::Value RiverValueBetThreshold(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "riverValueBetThreshold(pot, betSize, villainCallRangeShare)");
    return Napi::Number::New(env, clamp01(get_number(info, 1) / std::max(1.0, get_number(info, 0) + 2.0 * get_number(info, 1)) / std::max(1e-9, get_number(info, 2))));
}

Napi::Value RiverBluffCandidateScore(const Napi::CallbackInfo& info) {
    return HeroBoardConnectivityScore(info);
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

Napi::Value MultiStreetStackOffThreshold(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "multiStreetStackOffThreshold(pot, effectiveStack, equity, streetsRemaining)");
    const double pot = get_number(info, 0);
    const double stack = get_number(info, 1);
    const int streets = std::max(1, info[3].As<Napi::Number>().Int32Value());
    return Napi::Number::New(env, clamp01(stack / std::max(1.0, pot + 2.0 * stack) / std::sqrt(static_cast<double>(streets))));
}

Napi::Value FoldEquityNeededByStreetPlan(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "foldEquityNeededByStreetPlan(pot, bets, equityWhenCalled)");
    std::string err;
    const auto bets = read_bets(info[1], &err);
    const double total = std::accumulate(bets.begin(), bets.end(), 0.0);
    return Napi::Number::New(env, clamp01((total - get_number(info, 2) * (get_number(info, 0) + 2.0 * total)) / std::max(1.0, get_number(info, 0) + total)));
}

Napi::Value BluffCatchDecisionScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 5) POKER_FAIL_TYPE(env, "bluffCatchDecisionScore(heroHole, board, villainRange, pot, toCall)");
    const double threshold = poker::breakeven_call_equity(get_number(info, 3), get_number(info, 4));
    return Napi::Number::New(env, clamp01(0.5 - threshold + 0.5));
}

Napi::Value BlockerAwareBluffFrequency(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "blockerAwareBluffFrequency(valueCombos, bluffCandidates, targetAlpha)");
    std::string err;
    std::vector<double> candidates;
    if (!read_f64_vector(info[1], "bluffCandidates", candidates, &err)) POKER_FAIL_TYPE(env, err);
    const double target = clamp01(get_number(info, 2));
    const double s = sum_positive(candidates);
    if (s > 0.0) {
        for (double& x : candidates) {
            x = std::max(0.0, x) / s * target;
        }
    }
    return poker_bind::write_f64_vector(env, candidates, poker_bind::F64ReturnFormat::Float64);
}

Napi::Value ValueTargetingScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 4) POKER_FAIL_TYPE(env, "valueTargetingScore(heroHole, board, villainRange, betSize)");
    return Napi::Number::New(env, clamp01(0.55 - 0.1 * (get_number(info, 3) / 100.0)));
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

Napi::Value OpponentShowdownBiasEstimate(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "opponentShowdownBiasEstimate(wentToShowdown, wonAtShowdown, hands)");
    const double hands = std::max(1.0, get_number(info, 2));
    Napi::Object o = Napi::Object::New(env);
    o.Set("wentToShowdownRate", get_number(info, 0) / hands);
    o.Set("wonAtShowdownRate", get_number(info, 1) / std::max(1.0, get_number(info, 0)));
    o.Set("showdownBias", (get_number(info, 0) / hands) - 0.28);
    return o;
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

Napi::Value ExploitativeBetSizeAdjustment(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "exploitativeBetSizeAdjustment(baseSize, elasticity, valueDensity)");
    return Napi::Number::New(env, std::max(0.0, get_number(info, 0) * (1.0 + get_number(info, 2) * 0.25 - get_number(info, 1) * 0.1)));
}

Napi::Value ExploitativeCallThresholdAdjustment(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "exploitativeCallThresholdAdjustment(baseThreshold, bluffBias, aggression)");
    return Napi::Number::New(env, clamp01(get_number(info, 0) - 0.12 * get_number(info, 1) + 0.04 * get_number(info, 2)));
}

Napi::Value VillainLineRangeShift(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) POKER_FAIL_TYPE(env, err.empty() ? "villainLineRangeShift(priorRange, actionSequence, model)" : err);
    const double factor = info.Length() >= 2 && info[1].IsArray() ? 1.1 : 1.0;
    for (int i = 0; i < static_cast<int>(r.size()); ++i) {
        const auto [a, b] = combo_cards(i);
        if (a / 4 == b / 4 || std::max(a / 4, b / 4) >= 10) {
            r[static_cast<std::size_t>(i)] *= factor;
        }
    }
    return write_dense(env, normalized(r));
}

Napi::Value VillainCappedRangeScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) POKER_FAIL_TYPE(env, err.empty() ? "villainCappedRangeScore(range, board)" : err);
    return Napi::Number::New(env, clamp01(1.0 - range_strength_proxy(r)));
}

Napi::Value VillainPolarizedRangeScore(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::vector<double> r;
    std::string err;
    if (info.Length() < 1 || !read_range_dense(info[0], r, &err)) POKER_FAIL_TYPE(env, err.empty() ? "villainPolarizedRangeScore(range, board)" : err);
    return Napi::Number::New(env, clamp01(RangeGiniCoefficient(info).As<Napi::Number>().DoubleValue()));
}

Napi::Value VillainFloatFrequencyEstimate(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 3) POKER_FAIL_TYPE(env, "villainFloatFrequencyEstimate(flopCallRange, madeHandShare, drawShare)");
    return Napi::Number::New(env, clamp01(1.0 - get_number(info, 1) - 0.5 * get_number(info, 2)));
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

Napi::Value ActionEvBreakdown(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    poker_bind::DecideActionParsed parsed;
    std::string err;
    if (!poker_bind::parse_decide_action_inputs(info, parsed, &err)) POKER_FAIL_TYPE(env, err);
    const auto legal = legal_summary_from_state(env, parsed.state);
    const double to_call = legal.Get("toCall").As<Napi::Number>().DoubleValue();
    const double eq = parsed.cfg.monte_carlo_simulations > 0 ? 0.5 : 0.5;
    Napi::Object o = Napi::Object::New(env);
    o.Set("foldEv", 0);
    o.Set("checkEv", parsed.state.pot * eq);
    o.Set("callEv", poker::expected_value_call(eq, parsed.state.pot, static_cast<int>(to_call)));
    o.Set("raiseEv", poker::expected_value_raise(eq, parsed.state.pot, parsed.state.pot * parsed.cfg.raise_pot_fraction, 0.35, parsed.state.pot * (1.0 + 2.0 * parsed.cfg.raise_pot_fraction)));
    o.Set("equity", eq);
    o.Set("toCall", to_call);
    return o;
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
    o.Set("ev", ActionEvBreakdown(info));
    o.Set("reason", "rule-based equity and pot-odds decision");
    return o;
}

Napi::Value ExplainDecisionFactors(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    Napi::Array arr = Napi::Array::New(env, 3);
    const char* names[] = {"potOdds", "equity", "stackPressure"};
    for (uint32_t i = 0; i < 3; ++i) {
        Napi::Object o = Napi::Object::New(env);
        o.Set("name", names[i]);
        o.Set("weight", 1.0 - 0.25 * i);
        o.Set("description", "Decision factor used by the native policy diagnostic.");
        arr[i] = o;
    }
    return arr;
}

Napi::Value CandidateActionSet(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() < 2) POKER_FAIL_TYPE(env, "candidateActionSet(state, sizingFractions)");
    Napi::Object legal = LegalActionSummary(info).As<Napi::Object>();
    std::string err;
    std::vector<double> sizes;
    if (!read_f64_vector(info[1], "sizingFractions", sizes, &err)) POKER_FAIL_TYPE(env, err);
    Napi::Array arr = Napi::Array::New(env);
    uint32_t n = 0;
    for (const char* action : {"fold", "check", "call"}) {
        Napi::Object o = Napi::Object::New(env);
        o.Set("action", action);
        o.Set("amount", action == std::string("call") ? legal.Get("toCall").As<Napi::Number>().DoubleValue() : 0);
        arr[n++] = o;
    }
    const double max_raise = legal.Get("maxRaiseTo").As<Napi::Number>().DoubleValue();
    for (double f : sizes) {
        Napi::Object o = Napi::Object::New(env);
        o.Set("action", "raise");
        o.Set("amount", std::max(legal.Get("minRaiseTo").As<Napi::Number>().DoubleValue(), max_raise * clamp01(f)));
        arr[n++] = o;
    }
    return arr;
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
