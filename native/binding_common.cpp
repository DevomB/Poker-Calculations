#include "binding_common.hpp"

namespace poker_bind {

F64ReturnFormat parse_return_format(const Napi::CallbackInfo& info, std::size_t format_index) {
    if (info.Length() <= static_cast<int>(format_index)) {
        return F64ReturnFormat::Array;
    }
    const Napi::Value v = info[format_index];
    if (v.IsString()) {
        const std::string s = v.As<Napi::String>().Utf8Value();
        if (s == "float64") {
            return F64ReturnFormat::Float64;
        }
    }
    return F64ReturnFormat::Array;
}

bool is_abort_signal(const Napi::Value& value) {
    if (!value.IsObject()) {
        return false;
    }
    const Napi::Object obj = value.As<Napi::Object>();
    if (!obj.Has("aborted") || !obj.Get("aborted").IsBoolean()) {
        return false;
    }
    if (!obj.Has("addEventListener") || !obj.Get("addEventListener").IsFunction()) {
        return false;
    }
    return true;
}

bool is_async_options(const Napi::Value& value) {
    if (!value.IsObject()) {
        return false;
    }
    const Napi::Object obj = value.As<Napi::Object>();
    if (!obj.Has("signal")) {
        return false;
    }
    const Napi::Value sig = obj.Get("signal");
    if (sig.IsUndefined() || sig.IsNull()) {
        return true;
    }
    return is_abort_signal(sig);
}

int trailing_async_options_index(const Napi::CallbackInfo& info) {
    if (info.Length() < 1) {
        return -1;
    }
    if (is_async_options(info[info.Length() - 1])) {
        return static_cast<int>(info.Length()) - 1;
    }
    return -1;
}

int effective_arg_length(const Napi::CallbackInfo& info) {
    const int opts_idx = trailing_async_options_index(info);
    return opts_idx >= 0 ? opts_idx : static_cast<int>(info.Length());
}

Napi::Value parse_async_signal(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    const int idx = trailing_async_options_index(info);
    if (idx < 0) {
        return env.Undefined();
    }
    return info[idx].As<Napi::Object>().Get("signal");
}

Napi::Value make_abort_error(Napi::Env env) {
    const Napi::Value dom_ctor = env.Global().Get("DOMException");
    if (dom_ctor.IsFunction()) {
        return dom_ctor.As<Napi::Function>()
            .New({Napi::String::New(env, "This operation was aborted"),
                  Napi::String::New(env, "AbortError")});
    }
    Napi::Error err = Napi::Error::New(env, "This operation was aborted");
    err.Set("name", Napi::String::New(env, "AbortError"));
    err.Set("code", Napi::String::New(env, "ABORT_ERR"));
    return err.Value();
}

Napi::Promise reject_aborted_promise(Napi::Env env) {
    auto deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(make_abort_error(env));
    return deferred.Promise();
}

}  // namespace poker_bind
