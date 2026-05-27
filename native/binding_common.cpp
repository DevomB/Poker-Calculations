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

}  // namespace poker_bind
