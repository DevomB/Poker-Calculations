#include "binding_flop_cfr_buckets.hpp"

#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_range_input.hpp"
#include "binding_numeric.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/flop_cfr_buckets.hpp"
#include "poker/range.hpp"

#include <cstring>
#include <string>
#include <vector>

using poker_bind::parse_cards_from_js;
using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

constexpr int kCombo1326 = 1326;

bool parse_sparse_range(const Napi::Env /*env*/, const Napi::Value& v, const poker::DeckBitset& dead,
                        poker::SparseRange& out, std::string* err) {
    return poker_bind::parse_sparse_range_spec(v, dead.mask, out, err);
}

bool parse_i32_vector(const Napi::Value& v, const char* ctx, std::vector<int>& out, std::string* err) {
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        const std::size_t n = ta.ElementLength();
        out.resize(n);
        if (ta.TypedArrayType() == napi_int32_array) {
            std::memcpy(out.data(), poker_bind::typed_array_data(ta), n * sizeof(int32_t));
            return true;
        }
        if (ta.TypedArrayType() == napi_uint32_array) {
            const auto* src = static_cast<const std::uint32_t*>(poker_bind::typed_array_data(ta));
            for (std::size_t i = 0; i < n; ++i) {
                out[i] = static_cast<int>(src[i]);
            }
            return true;
        }
        if (err) {
            *err = std::string(ctx) + " must be Int32Array";
        }
        return false;
    }
    if (v.IsArray()) {
        const Napi::Array a = v.As<Napi::Array>();
        out.resize(a.Length());
        for (uint32_t i = 0; i < a.Length(); ++i) {
            out[i] = poker_bind::checked_int32(a.Get(i), "bucket");
        }
        return true;
    }
    if (err) {
        *err = std::string(ctx) + " must be Int32Array";
    }
    return false;
}

Napi::Int32Array write_i32_vector(Napi::Env env, const std::vector<int>& data) {
    Napi::Int32Array arr = Napi::Int32Array::New(env, data.size());
    if (!data.empty()) {
        std::memcpy(arr.Data(), data.data(), data.size() * sizeof(int32_t));
    }
    return arr;
}

poker::DeckBitset dead_from_cards(const std::vector<poker::Card>& cards) {
    poker::DeckBitset dead;
    dead.mark_cards(cards);
    return dead;
}

}  // namespace

Napi::Value FlopBucketCountDefault(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    return Napi::Number::New(env, poker::flop_bucket_count_default());
}

Napi::Value Ehs2BucketsVsRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "ehs2BucketsVsRange(board, range[, k][, { trials, seed }])");
    std::string err;
    const std::vector<poker::Card> board = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    poker::SparseRange range;
    if (!parse_sparse_range(env, info[1], dead_from_cards(board), range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    int k = poker::flop_bucket_count_default();
    poker::ComboEhsTableOptions opt{};
    std::size_t idx = 2;
    if (idx < info.Length() && info[idx].IsNumber()) {
        k = poker_bind::checked_int32(info[idx], "k");
        ++idx;
    }
    if (idx < info.Length() && info[idx].IsObject() && !info[idx].IsTypedArray()) {
        const Napi::Object o = info[idx].As<Napi::Object>();
        if (o.Has("trials") && o.Get("trials").IsNumber()) {
            const int t = poker_bind::checked_int32(o.Get("trials"), "trials");
            opt.trials = t < 0 ? 0 : static_cast<std::size_t>(t);
        }
        if (o.Has("seed") && o.Get("seed").IsNumber()) {
            opt.seed = o.Get("seed").As<Napi::Number>().Uint32Value();
        }
        if (o.Has("bucketCount") && o.Get("bucketCount").IsNumber()) {
            k = poker_bind::checked_int32(o.Get("bucketCount"), "bucketCount");
        }
    }
    POKER_TRY(env, {
        const std::vector<int> buckets = poker::ehs2_buckets_vs_range(board, range, k, opt);
        return write_i32_vector(env, buckets);
    });
}

Napi::Value BucketMassFromRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "bucketMassFromRange(range, buckets[, k])");
    std::string err;
    poker::SparseRange range;
    poker::DeckBitset empty;
    if (!parse_sparse_range(env, info[0], empty, range, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<int> buckets;
    if (!parse_i32_vector(info[1], "buckets", buckets, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    int k = poker::flop_bucket_count_default();
    if (info.Length() >= 3 && info[2].IsNumber()) {
        k = poker_bind::checked_int32(info[2], "k");
    } else {
        int mx = 0;
        for (int b : buckets) {
            if (b > mx) {
                mx = b;
            }
        }
        k = mx + 1;
        if (k < 1) {
            k = 1;
        }
    }
    POKER_TRY(env, {
        return write_f64_vector(env, poker::bucket_mass_from_range(range, buckets, k),
                                poker_bind::F64ReturnFormat::Float64);
    });
}
Napi::Value FlopBucketStrategyTo1326(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "flopBucketStrategyTo1326(bucketMix, comboBuckets)");
    std::string err;
    std::vector<double> mix;
    std::vector<int> buckets;
    if (!read_f64_vector(info[0], "bucketMix", mix, &err) ||
        !parse_i32_vector(info[1], "comboBuckets", buckets, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        return write_f64_vector(env, poker::flop_bucket_strategy_to_1326(mix, buckets),
                                poker_bind::F64ReturnFormat::Float64);
    });
}

Napi::Value CanonicalFlopCfrKey(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "canonicalFlopCfrKey(flop, pot[, stack])");
    std::string err;
    const std::vector<poker::Card> flop = parse_cards_from_js(env, info[0], &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    const double pot = info[1].As<Napi::Number>().DoubleValue();
    const double stack = info.Length() >= 3 && info[2].IsNumber() ? info[2].As<Napi::Number>().DoubleValue() : 0.0;
    POKER_TRY(env, { return Napi::String::New(env, poker::canonical_flop_cfr_key(flop, pot, stack)); });
}
