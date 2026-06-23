#include "binding_combinatorics.hpp"

#include "async_workers.hpp"
#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/combinatorics_exact.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/range.hpp"

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;

namespace {

bool parse_sparse_range(const Napi::Env env, const Napi::Value& v, const poker::DeckBitset& dead,
                        poker::SparseRange& out, std::string* err) {
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_float64_array || ta.ElementLength() != 1326) {
            if (err) {
                *err = "dense range must be Float64Array of length 1326";
            }
            return false;
        }
        std::vector<double> w(1326);
        std::memcpy(w.data(), ta.ArrayBuffer().Data(), 1326 * sizeof(double));
        out = poker::sparse_range_from_dense1326(w.data(), 1326, dead.mask);
        return true;
    }
    if (!v.IsObject()) {
        if (err) {
            *err = "range must be Float64Array(1326) or { indices, weights }";
        }
        return false;
    }
    const Napi::Object o = v.As<Napi::Object>();
    std::vector<int> indices;
    std::vector<double> weights;
    const Napi::Value iv = o.Get("indices");
    if (!iv.IsTypedArray()) {
        if (err) {
            *err = "sparse range needs indices Int32Array";
        }
        return false;
    }
    const Napi::TypedArray ta = iv.As<Napi::TypedArray>();
    const std::size_t n = ta.ElementLength();
    indices.resize(n);
    std::memcpy(indices.data(), ta.ArrayBuffer().Data(), n * sizeof(int32_t));
    if (!read_f64_vector(o.Get("weights"), "weights", weights, err)) {
        return false;
    }
    out = poker::sparse_range_from_arrays(indices, weights, dead.mask);
    return true;
}

Napi::Object vulnerability_to_js(Napi::Env env, const poker::HeroRunoutVulnerabilityResult& r) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("pNuts", Napi::Number::New(env, r.p_nuts));
    o.Set("pDominated", Napi::Number::New(env, r.p_dominated));
    o.Set("runoutCount", Napi::Number::New(env, static_cast<double>(r.runout_count)));
    return o;
}

Napi::Object quantiles_to_js(Napi::Env env, const poker::HeroEquityRunoutQuantilesResult& r) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("mean", Napi::Number::New(env, r.mean));
    o.Set("variance", Napi::Number::New(env, r.variance));
    o.Set("p05", Napi::Number::New(env, r.p05));
    o.Set("p50", Napi::Number::New(env, r.p50));
    o.Set("p95", Napi::Number::New(env, r.p95));
    o.Set("n", Napi::Number::New(env, static_cast<double>(r.n)));
    return o;
}

}  // namespace

Napi::Value ExactHeroRunoutVulnerability(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHeroRunoutVulnerability(heroHoleCards, boardCards[, knownDead])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (info.Length() >= 3 && !info[2].IsUndefined()) {
        dead = parse_cards_from_js(env, info[2], &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    POKER_TRY(env, {
        return vulnerability_to_js(env,
                                   poker::exact_hero_runout_vulnerability(hero, board, dead));
    });
}

Napi::Value ExactHeroRunoutVulnerabilityAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHeroRunoutVulnerabilityAsync(heroHoleCards, boardCards[, knownDead][, options])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (info.Length() >= 3 && !info[2].IsUndefined() && !info[2].IsNull() &&
        !poker_bind::is_async_options(info[2])) {
        dead = parse_cards_from_js(env, info[2], &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    (void)poker_bind::parse_async_signal(info);
    auto deferred = Napi::Promise::Deferred::New(env);
    class Worker : public Napi::AsyncWorker {
     public:
        Worker(Napi::Promise::Deferred d, std::vector<poker::Card> hero,
               std::vector<poker::Card> board, std::vector<poker::Card> dead)
            : Napi::AsyncWorker(d.Env()),
              deferred_(d),
              hero_(std::move(hero)),
              board_(std::move(board)),
              dead_(std::move(dead)) {}

        void Execute() override {
            result_ = poker::exact_hero_runout_vulnerability(hero_, board_, dead_);
        }

        void OnOK() override {
            Napi::HandleScope scope(Env());
            deferred_.Resolve(vulnerability_to_js(Env(), result_));
        }

        void OnError(const Napi::Error& e) override {
            Napi::HandleScope scope(Env());
            deferred_.Reject(e.Value());
        }

     private:
        Napi::Promise::Deferred deferred_;
        std::vector<poker::Card> hero_;
        std::vector<poker::Card> board_;
        std::vector<poker::Card> dead_;
        poker::HeroRunoutVulnerabilityResult result_{};
    };
    auto worker = std::make_unique<Worker>(deferred, hero, board, dead);
    worker->Queue();
    worker.release();
    return deferred.Promise();
}

Napi::Value ExactVillainLeapfrogOutCounts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactVillainLeapfrogOutCounts(heroHoleCards, boardCards[, knownDead])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (info.Length() >= 3 && !info[2].IsUndefined()) {
        dead = parse_cards_from_js(env, info[2], &err);
    }
    POKER_TRY(env, {
        const auto r = poker::exact_villain_leapfrog_out_counts(hero, board, dead);
        Napi::Object o = Napi::Object::New(env);
        auto to_arr = [&](const std::vector<int>& v) {
            Napi::Array a = Napi::Array::New(env, v.size());
            for (std::size_t i = 0; i < v.size(); ++i) {
                a.Set(static_cast<uint32_t>(i), Napi::Number::New(env, v[i]));
            }
            return a;
        };
        o.Set("leapfrogDeckIndices", to_arr(r.leapfrog_deck_indices));
        o.Set("heroImproveDeckIndices", to_arr(r.hero_improve_deck_indices));
        return o;
    });
}

Napi::Value ExactHeroCategoryJointFlopToRiver(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHeroCategoryJointFlopToRiver(heroHoleCards, flopThree[, knownDead])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> flop = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<poker::Card> dead;
    if (info.Length() >= 3 && !info[2].IsUndefined()) {
        dead = parse_cards_from_js(env, info[2], &err);
    }
    POKER_TRY(env, {
        const auto mat = poker::exact_hero_category_joint_flop_to_river(hero, flop, dead);
        Napi::Float64Array arr = Napi::Float64Array::New(env, mat.size());
        for (std::size_t i = 0; i < mat.size(); ++i) {
            arr.Set(i, mat[i]);
        }
        Napi::Object o = Napi::Object::New(env);
        o.Set("jointMatrix", arr);
        return o;
    });
}

Napi::Value ExactRangeDominatedComboFraction(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactRangeDominatedComboFraction(heroHoleCards, boardCards, range)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    poker::DeckBitset dead;
    dead.mark_cards(hero);
    dead.mark_cards(board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return Napi::Number::New(env, poker::exact_range_dominated_combo_fraction(hero, board, range));
    });
}

Napi::Value ExactHeroEquityRunoutQuantiles(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHeroEquityRunoutQuantiles(heroHoleCards, boardCards[, range])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        if (info.Length() >= 3 && !info[2].IsUndefined() && !info[2].IsNull()) {
            poker::SparseRange range;
            poker::DeckBitset dead;
            dead.mark_cards(hero);
            dead.mark_cards(board);
            if (!parse_sparse_range(env, info[2], dead, range, &err)) {
                POKER_FAIL_TYPE(env, err);
            }
            return quantiles_to_js(env,
                                   poker::exact_hero_equity_runout_quantiles_vs_range(hero, board, range));
        }
        return quantiles_to_js(env, poker::exact_hero_equity_runout_quantiles_vs_random(hero, board, {}));
    });
}

Napi::Value ExactHeroEquityRunoutQuantilesAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2,
                  "exactHeroEquityRunoutQuantilesAsync(heroHoleCards, boardCards[, range][, options])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    bool vs_range = false;
    poker::SparseRange range;
    if (info.Length() >= 3 && !info[2].IsUndefined() && !info[2].IsNull() &&
        !poker_bind::is_async_options(info[2])) {
        vs_range = true;
        poker::DeckBitset dead;
        dead.mark_cards(hero);
        dead.mark_cards(board);
        if (!parse_sparse_range(env, info[2], dead, range, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    auto deferred = Napi::Promise::Deferred::New(env);
    class Worker : public Napi::AsyncWorker {
     public:
        Worker(Napi::Promise::Deferred d, std::vector<poker::Card> hero,
               std::vector<poker::Card> board, bool vs_range, poker::SparseRange range)
            : Napi::AsyncWorker(d.Env()),
              deferred_(d),
              hero_(std::move(hero)),
              board_(std::move(board)),
              vs_range_(vs_range),
              range_(std::move(range)) {}

        void Execute() override {
            if (vs_range_) {
                result_ = poker::exact_hero_equity_runout_quantiles_vs_range(hero_, board_, range_);
            } else {
                result_ = poker::exact_hero_equity_runout_quantiles_vs_random(hero_, board_, {});
            }
        }

        void OnOK() override {
            Napi::HandleScope scope(Env());
            deferred_.Resolve(quantiles_to_js(Env(), result_));
        }

        void OnError(const Napi::Error& e) override {
            Napi::HandleScope scope(Env());
            deferred_.Reject(e.Value());
        }

     private:
        Napi::Promise::Deferred deferred_;
        std::vector<poker::Card> hero_;
        std::vector<poker::Card> board_;
        bool vs_range_{false};
        poker::SparseRange range_;
        poker::HeroEquityRunoutQuantilesResult result_{};
    };
    auto worker = std::make_unique<Worker>(deferred, hero, board, vs_range, range);
    worker->Queue();
    worker.release();
    return deferred.Promise();
}

Napi::Value ExactEquityCardRemovalGradient(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactEquityCardRemovalGradient(heroHoleCards, boardCards, range)");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    poker::DeckBitset dead;
    dead.mark_cards(hero);
    dead.mark_cards(board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::exact_equity_card_removal_gradient(hero, board, range);
        Napi::Object o = Napi::Object::New(env);
        Napi::Float64Array g = Napi::Float64Array::New(env, 52);
        for (int i = 0; i < 52; ++i) {
            g.Set(static_cast<uint32_t>(i), r.gradient[static_cast<std::size_t>(i)]);
        }
        o.Set("gradient", g);
        o.Set("baseEquity", Napi::Number::New(env, r.base_equity));
        return o;
    });
}

Napi::Value ExactEquityCardRemovalGradientAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "exactEquityCardRemovalGradientAsync(heroHoleCards, boardCards, range[, options])");
    std::string err;
    const std::vector<poker::Card> hero = parse_cards_from_js(env, info[0], &err);
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[1], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    poker::DeckBitset dead;
    dead.mark_cards(hero);
    dead.mark_cards(board);
    if (!parse_sparse_range(env, info[2], dead, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    auto deferred = Napi::Promise::Deferred::New(env);
    class Worker : public Napi::AsyncWorker {
     public:
        Worker(Napi::Promise::Deferred d, std::vector<poker::Card> hero,
               std::vector<poker::Card> board, poker::SparseRange range)
            : Napi::AsyncWorker(d.Env()),
              deferred_(d),
              hero_(std::move(hero)),
              board_(std::move(board)),
              range_(std::move(range)) {}

        void Execute() override {
            result_ = poker::exact_equity_card_removal_gradient(hero_, board_, range_);
        }

        void OnOK() override {
            Napi::HandleScope scope(Env());
            Napi::Object o = Napi::Object::New(Env());
            Napi::Float64Array g = Napi::Float64Array::New(Env(), 52);
            for (int i = 0; i < 52; ++i) {
                g.Set(static_cast<uint32_t>(i), result_.gradient[static_cast<std::size_t>(i)]);
            }
            o.Set("gradient", g);
            o.Set("baseEquity", Napi::Number::New(Env(), result_.base_equity));
            deferred_.Resolve(o);
        }

        void OnError(const Napi::Error& e) override {
            deferred_.Reject(e.Value());
        }

     private:
        Napi::Promise::Deferred deferred_;
        std::vector<poker::Card> hero_;
        std::vector<poker::Card> board_;
        poker::SparseRange range_;
        poker::CardRemovalGradientResult result_{};
    };
    auto worker = std::make_unique<Worker>(deferred, hero, board, range);
    worker->Queue();
    worker.release();
    return deferred.Promise();
}
