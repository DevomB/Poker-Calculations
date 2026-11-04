#include "binding_range_input.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include <cmath>
#include <cstring>
#include <exception>

namespace poker_bind {

namespace {

constexpr const char* kRangeShape = "range must be Float64Array(1326) or { indices, weights }";
constexpr const char* kIndicesShape = "indices must be number[], Int32Array, or Uint32Array of deck ids 0..51";

bool fail(std::string* err, std::string msg) {
    if (err) {
        *err = std::move(msg);
    }
    return false;
}

bool bad_index(std::string* err, std::size_t i) {
    return fail(err, "indices[" + std::to_string(i) + "] must be an integer deck id 0..51");
}

bool read_dense_f64(const Napi::Value& v, std::vector<double>& out, std::string* err) {
    const Napi::TypedArray ta = v.As<Napi::TypedArray>();
    if (ta.TypedArrayType() != napi_float64_array || ta.ElementLength() != static_cast<std::size_t>(poker::kHoleComboCount)) {
        return fail(err, "dense range must be Float64Array of length 1326");
    }
    out.resize(static_cast<std::size_t>(poker::kHoleComboCount));
    std::memcpy(out.data(), typed_array_data(ta), out.size() * sizeof(double));
    return true;
}

bool read_sparse_parts(const Napi::Value& v, std::vector<int>& indices, std::vector<double>& weights,
                       std::string* err) {
    if (!v.IsObject() || v.IsArray()) {
        return fail(err, kRangeShape);
    }
    const Napi::Object o = v.As<Napi::Object>();
    if (!o.Has("indices") || !o.Has("weights")) {
        return fail(err, "sparse range object needs indices and weights");
    }
    if (!read_range_deck_ids(o.Get("indices"), indices, err)) {
        return false;
    }
    if (!read_f64_vector(o.Get("weights"), "weights", weights, err)) {
        return false;
    }
    if (indices.size() % 2 != 0 || weights.size() != indices.size() / 2) {
        return fail(err, "sparse range needs two deck ids in indices per entry of weights");
    }
    return true;
}

}  // namespace

bool read_range_deck_ids(const Napi::Value& v, std::vector<int>& out, std::string* err) {
    out.clear();
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        const std::size_t n = ta.ElementLength();
        out.resize(n);
        if (ta.TypedArrayType() == napi_int32_array) {
            const auto* src = static_cast<const std::int32_t*>(typed_array_data(ta));
            for (std::size_t i = 0; i < n; ++i) {
                if (src[i] < 0 || src[i] > 51) {
                    return bad_index(err, i);
                }
                out[i] = src[i];
            }
            return true;
        }
        if (ta.TypedArrayType() == napi_uint32_array) {
            const auto* src = static_cast<const std::uint32_t*>(typed_array_data(ta));
            for (std::size_t i = 0; i < n; ++i) {
                if (src[i] > 51U) {
                    return bad_index(err, i);
                }
                out[i] = static_cast<int>(src[i]);
            }
            return true;
        }
        return fail(err, kIndicesShape);
    }
    if (!v.IsArray()) {
        return fail(err, kIndicesShape);
    }
    const Napi::Array a = v.As<Napi::Array>();
    const std::uint32_t n = a.Length();
    out.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        const Napi::Value x = a.Get(i);
        if (!x.IsNumber()) {
            return bad_index(err, i);
        }
        const double d = x.As<Napi::Number>().DoubleValue();
        // Checked as a double: NaN, fractions, and values past 2^31 never reach an integer conversion.
        if (!(d >= 0.0 && d <= 51.0) || std::floor(d) != d) {
            return bad_index(err, i);
        }
        out.push_back(static_cast<int>(d));
    }
    return true;
}

bool parse_sparse_range_spec(const Napi::Value& v, std::uint64_t dead_mask, poker::SparseRange& out,
                             std::string* err) {
    try {
        if (v.IsTypedArray()) {
            std::vector<double> dense;
            if (!read_dense_f64(v, dense, err)) {
                return false;
            }
            out = poker::sparse_range_from_dense1326(dense.data(), dense.size(), dead_mask);
            return true;
        }
        std::vector<int> indices;
        std::vector<double> weights;
        if (!read_sparse_parts(v, indices, weights, err)) {
            return false;
        }
        out = poker::sparse_range_from_arrays(indices, weights, dead_mask);
        return true;
    } catch (const std::exception& e) {
        return fail(err, e.what());
    }
}

bool read_dense_range_spec(const Napi::Value& v, std::vector<double>& out, std::string* err) {
    if (v.IsTypedArray()) {
        if (!read_dense_f64(v, out, err)) {
            return false;
        }
        for (const double w : out) {
            if (!std::isfinite(w) || w < 0.0) {
                return fail(err, "range weights must be finite and non-negative");
            }
        }
        return true;
    }
    std::vector<int> indices;
    std::vector<double> weights;
    if (!read_sparse_parts(v, indices, weights, err)) {
        return false;
    }
    out.assign(static_cast<std::size_t>(poker::kHoleComboCount), 0.0);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const int idx = poker::combo_index_1326(indices[i * 2], indices[i * 2 + 1]);
        if (idx < 0) {
            return fail(err, "invalid deck index in sparse range");
        }
        const double w = weights[i];
        if (!std::isfinite(w) || w < 0.0) {
            return fail(err, "range weights must be finite and non-negative");
        }
        double& slot = out[static_cast<std::size_t>(idx)];
        slot += w;  // a combo listed more than once adds up, as in the sparse builders
        if (!std::isfinite(slot)) {
            return fail(err, "range weights are too large: a combo listed more than once sums past the largest double");
        }
    }
    return true;
}

}  // namespace poker_bind
