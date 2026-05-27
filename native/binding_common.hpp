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

#define POKER_TRY(env, body)                          \
    try {                                             \
        body                                          \
    } catch (const std::exception& e) {               \
        Napi::Error::New(env, e.what())               \
            .ThrowAsJavaScriptException();            \
        return env.Null();                            \
    }

}  // namespace poker_bind
