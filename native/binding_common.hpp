#pragma once

#include <napi.h>

#include <string>

namespace poker_bind {

enum class F64ReturnFormat { Array, Float64 };

enum class BindStatus { Ok, TypeError, RangeError };

[[nodiscard]] inline bool fail_type(Napi::Env env, const std::string& msg) {
    Napi::TypeError::New(env, msg).ThrowAsJavaScriptException();
    return false;
}

[[nodiscard]] inline bool fail_range(Napi::Env env, const std::string& msg) {
    Napi::RangeError::New(env, msg).ThrowAsJavaScriptException();
    return false;
}

[[nodiscard]] F64ReturnFormat parse_return_format(const Napi::CallbackInfo& info, std::size_t format_index);

[[nodiscard]] bool is_abort_signal(const Napi::Value& v);

[[nodiscard]] bool is_async_options(const Napi::Value& v);

/** Index of trailing `{ signal }` options object, or -1. */
[[nodiscard]] int trailing_async_options_index(const Napi::CallbackInfo& info);

/** Argument count excluding trailing async options. */
[[nodiscard]] int effective_arg_length(const Napi::CallbackInfo& info);

/** `AbortSignal` from trailing async options, or `undefined`. */
[[nodiscard]] Napi::Value parse_async_signal(const Napi::CallbackInfo& info);

[[nodiscard]] Napi::Value make_abort_error(Napi::Env env);

[[nodiscard]] Napi::Promise reject_aborted_promise(Napi::Env env);

#define POKER_TRY(env, body)                          \
    try {                                             \
        body                                          \
    } catch (const std::exception& e) {               \
        Napi::Error::New(env, e.what())               \
            .ThrowAsJavaScriptException();            \
        return env.Null();                            \
    }

}  // namespace poker_bind
