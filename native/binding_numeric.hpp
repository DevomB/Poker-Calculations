#pragma once

#include "binding_common.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace poker_bind {

[[nodiscard]] bool read_f64_vector(const Napi::Value& v, const char* ctx, std::vector<double>& out,
                                   std::string* err);

[[nodiscard]] bool read_f64_matrix(const Napi::Value& v, const char* ctx, std::vector<std::vector<double>>& out,
                                   std::string* err);

[[nodiscard]] bool read_f64_matrix_flat(const Napi::Value& data_v, int cols, const char* ctx,
                                        std::vector<std::vector<double>>& out, std::string* err);

[[nodiscard]] Napi::Value write_f64_vector(Napi::Env env, const std::vector<double>& data, F64ReturnFormat fmt);

[[nodiscard]] Napi::Value write_f64_vector(Napi::Env env, const double* data, std::size_t n, F64ReturnFormat fmt);

[[nodiscard]] Napi::Value write_f64_matrix_flat(Napi::Env env, const std::vector<std::vector<double>>& mat,
                                                F64ReturnFormat fmt);

[[nodiscard]] bool write_f64_into_out(Napi::Env env, const Napi::Value& out_arg, const double* data, std::size_t n,
                                    std::string* err);

// Legacy helpers (Array-only); prefer read_f64_vector for new code.
[[nodiscard]] std::vector<double> doubles_from_js_array(const Napi::Array& a, const char* ctx);

[[nodiscard]] std::vector<std::vector<double>> matrix_from_js_array(const Napi::Array& rows, const char* ctx);

}  // namespace poker_bind
