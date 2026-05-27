#include "binding_numeric.hpp"

#include <cstring>
#include <stdexcept>

namespace poker_bind {

namespace {

bool read_f64_from_typed(const Napi::TypedArray& ta, const char* ctx, std::vector<double>& out, std::string* err) {
    if (ta.TypedArrayType() != napi_float64_array) {
        if (err) {
            *err = std::string(ctx) + ": Float64Array expected";
        }
        return false;
    }
    const std::size_t n = ta.ElementLength();
    out.resize(n);
    if (n == 0) {
        return true;
    }
    Napi::ArrayBuffer ab = ta.ArrayBuffer();
    const auto* src = static_cast<const double*>(static_cast<void*>(static_cast<std::uint8_t*>(ab.Data()) + ta.ByteOffset()));
    std::memcpy(out.data(), src, n * sizeof(double));
    return true;
}

}  // namespace

bool read_f64_vector(const Napi::Value& v, const char* ctx, std::vector<double>& out, std::string* err) {
    out.clear();
    if (v.IsArray()) {
        try {
            out = doubles_from_js_array(v.As<Napi::Array>(), ctx);
            return true;
        } catch (const std::exception& e) {
            if (err) {
                *err = e.what();
            }
            return false;
        }
    }
    if (v.IsTypedArray()) {
        return read_f64_from_typed(v.As<Napi::TypedArray>(), ctx, out, err);
    }
    if (err) {
        *err = std::string(ctx) + ": expected number[] or Float64Array";
    }
    return false;
}

bool read_f64_matrix(const Napi::Value& v, const char* ctx, std::vector<std::vector<double>>& out, std::string* err) {
    out.clear();
    if (v.IsArray()) {
        try {
            out = matrix_from_js_array(v.As<Napi::Array>(), ctx);
            return true;
        } catch (const std::exception& e) {
            if (err) {
                *err = e.what();
            }
            return false;
        }
    }
    if (v.IsTypedArray()) {
        if (err) {
            *err = std::string(ctx) + ": flat matrix use read_f64_matrix_flat(data, cols, ...)";
        }
        return false;
    }
    if (err) {
        *err = std::string(ctx) + ": expected number[][] or Float64Array";
    }
    return false;
}

bool read_f64_matrix_flat(const Napi::Value& data_v, int cols, const char* ctx,
                          std::vector<std::vector<double>>& out, std::string* err) {
    out.clear();
    if (cols < 1) {
        if (err) {
            *err = std::string(ctx) + ": cols must be >= 1";
        }
        return false;
    }
    std::vector<double> flat;
    if (!read_f64_vector(data_v, ctx, flat, err)) {
        return false;
    }
    if (flat.size() % static_cast<std::size_t>(cols) != 0) {
        if (err) {
            *err = std::string(ctx) + ": flat length must be multiple of cols";
        }
        return false;
    }
    const std::size_t rows = flat.size() / static_cast<std::size_t>(cols);
    out.resize(rows);
    for (std::size_t r = 0; r < rows; ++r) {
        out[r].resize(static_cast<std::size_t>(cols));
        for (int c = 0; c < cols; ++c) {
            out[r][static_cast<std::size_t>(c)] = flat[r * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)];
        }
    }
    return true;
}

Napi::Value write_f64_vector(Napi::Env env, const std::vector<double>& data, F64ReturnFormat fmt) {
    return write_f64_vector(env, data.data(), data.size(), fmt);
}

Napi::Value write_f64_vector(Napi::Env env, const double* data, std::size_t n, F64ReturnFormat fmt) {
    if (fmt == F64ReturnFormat::Float64) {
        if (n == 0) {
            return Napi::Float64Array::New(env, 0);
        }
        Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, n * sizeof(double));
        std::memcpy(ab.Data(), data, n * sizeof(double));
        return Napi::Float64Array::New(env, n, ab, 0);
    }
    Napi::Array a = Napi::Array::New(env, static_cast<uint32_t>(n));
    for (std::size_t i = 0; i < n; ++i) {
        a[static_cast<uint32_t>(i)] = Napi::Number::New(env, data[i]);
    }
    return a;
}

Napi::Value write_f64_matrix_flat(Napi::Env env, const std::vector<std::vector<double>>& mat, F64ReturnFormat fmt) {
    if (mat.empty()) {
        return write_f64_vector(env, std::vector<double>{}, fmt);
    }
    const std::size_t cols = mat[0].size();
    std::vector<double> flat;
    flat.reserve(mat.size() * cols);
    for (const auto& row : mat) {
        flat.insert(flat.end(), row.begin(), row.end());
    }
    return write_f64_vector(env, flat, fmt);
}

bool write_f64_into_out(Napi::Env env, const Napi::Value& out_arg, const double* data, std::size_t n,
                        std::string* err) {
    if (out_arg.IsEmpty() || out_arg.IsUndefined()) {
        return true;
    }
    if (!out_arg.IsTypedArray()) {
        if (err) {
            *err = "out must be a Float64Array";
        }
        return false;
    }
    const Napi::TypedArray ta = out_arg.As<Napi::TypedArray>();
    if (ta.TypedArrayType() != napi_float64_array) {
        if (err) {
            *err = "out must be a Float64Array";
        }
        return false;
    }
    if (ta.ElementLength() != n) {
        if (err) {
            *err = "out Float64Array length mismatch";
        }
        return false;
    }
    Napi::ArrayBuffer ab = ta.ArrayBuffer();
    auto* dst = static_cast<double*>(static_cast<void*>(static_cast<std::uint8_t*>(ab.Data()) + ta.ByteOffset()));
    std::memcpy(dst, data, n * sizeof(double));
    return true;
}

std::vector<double> doubles_from_js_array(const Napi::Array& a, const char* ctx) {
    std::vector<double> v;
    const uint32_t n = a.Length();
    v.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Napi::Value x = a[i];
        if (!x.IsNumber()) {
            throw std::invalid_argument(std::string(ctx) + ": array must contain only numbers");
        }
        v.push_back(x.As<Napi::Number>().DoubleValue());
    }
    return v;
}

std::vector<std::vector<double>> matrix_from_js_array(const Napi::Array& rows, const char* ctx) {
    std::vector<std::vector<double>> m;
    const uint32_t rn = rows.Length();
    m.reserve(rn);
    for (uint32_t r = 0; r < rn; ++r) {
        const Napi::Value rv = rows[r];
        if (!rv.IsArray()) {
            throw std::invalid_argument(std::string(ctx) + ": expected array of arrays");
        }
        m.push_back(doubles_from_js_array(rv.As<Napi::Array>(), ctx));
    }
    return m;
}

}  // namespace poker_bind
