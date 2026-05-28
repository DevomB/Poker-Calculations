#include "binding_cards.hpp"

#include "binding_common.hpp"
#include "binding_init.hpp"
#include "poker/card_string.hpp"
#include "poker/fast_evaluator.hpp"

#include <array>
#include <stdexcept>
#include <vector>

namespace poker_bind {

namespace {

constexpr std::size_t kMaxCardUtf8Len = 15;

bool parse_card_string_from_napi_value(const Napi::Env& env, const Napi::Value& v, poker::Card& out,
                                       std::string* err) {
    if (!v.IsString()) {
        if (err) {
            *err = "expected string card";
        }
        return false;
    }
    return parse_card_string_from_js(env, v.As<Napi::String>(), out, err);
}

}  // namespace

bool parse_card_string_from_js(const Napi::Env& env, const Napi::String& str, poker::Card& out, std::string* err) {
    napi_env nenv = env;
    napi_value val = str;
    size_t len = 0;
    napi_status st = napi_get_value_string_utf8(nenv, val, nullptr, 0, &len);
    if (st != napi_ok) {
        if (err) {
            *err = "invalid card string";
        }
        return false;
    }
    if (len > kMaxCardUtf8Len) {
        if (err) {
            *err = "invalid card string";
        }
        return false;
    }
    std::vector<char> buf(len + 1);
    size_t written = 0;
    st = napi_get_value_string_utf8(nenv, val, buf.data(), buf.size(), &written);
    if (st != napi_ok) {
        if (err) {
            *err = "invalid card string";
        }
        return false;
    }
    if (!poker::parse_card_string_unchecked(buf.data(), written, out)) {
        if (err) {
            *err = "invalid card string";
        }
        return false;
    }
    return true;
}

std::vector<poker::Card> parse_card_strings(const Napi::Env& env, const Napi::Array& arr, std::string* err) {
    std::vector<poker::Card> out;
    const uint32_t n = arr.Length();
    out.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Napi::Value v = arr[i];
        if (!v.IsString()) {
            if (err) {
                *err = "cards must be strings like \"Ah\" or a Uint8Array of deck ids 0..51";
            }
            return {};
        }
        poker::Card c;
        if (!parse_card_string_from_js(env, v.As<Napi::String>(), c, err)) {
            if (err) {
                *err = "invalid card at index " + std::to_string(i);
            }
            return {};
        }
        out.push_back(c);
    }
    return out;
}

bool is_card_input(const Napi::Value& v) {
    if (v.IsArray()) {
        return true;
    }
    if (v.IsBuffer()) {
        return true;
    }
    if (v.IsTypedArray()) {
        return v.As<Napi::TypedArray>().TypedArrayType() == napi_uint8_array;
    }
    return false;
}

bool packed_card_bytes(const Napi::Value& v, const std::uint8_t** data, std::size_t* len, std::string* err) {
    if (v.IsBuffer()) {
        const Napi::Buffer<std::uint8_t> buf = v.As<Napi::Buffer<std::uint8_t>>();
        *data = buf.Data();
        *len = buf.Length();
        return true;
    }
    if (v.IsTypedArray()) {
        const Napi::TypedArray ta = v.As<Napi::TypedArray>();
        if (ta.TypedArrayType() != napi_uint8_array) {
            if (err) {
                *err = "packed cards must be Uint8Array (each byte 0..51)";
            }
            return false;
        }
        Napi::ArrayBuffer ab = ta.ArrayBuffer();
        *data = static_cast<const std::uint8_t*>(ab.Data()) + ta.ByteOffset();
        *len = ta.ByteLength();
        return true;
    }
    return false;
}

std::vector<poker::Card> parse_cards_from_js(const Napi::Env& env, const Napi::Value& v, std::string* err) {
    const std::uint8_t* data = nullptr;
    std::size_t len = 0;
    if (packed_card_bytes(v, &data, &len, err)) {
        std::vector<poker::Card> out;
        if (!poker::parse_packed_cards(data, len, out, err)) {
            return {};
        }
        return out;
    }
    if (v.IsArray()) {
        return parse_card_strings(env, v.As<Napi::Array>(), err);
    }
    if (err) {
        *err = "cards must be string[] or Uint8Array of deck indices (0..51)";
    }
    return {};
}

bool js_card_array_has_duplicate(const Napi::Env& env, const Napi::Array& arr, std::string* err) {
    bool seen[52]{};
    const uint32_t n = arr.Length();
    for (uint32_t i = 0; i < n; ++i) {
        poker::Card c{};
        if (!parse_card_string_from_napi_value(env, arr[i], c, err)) {
            if (err) {
                *err = "invalid card at index " + std::to_string(i);
            }
            return false;
        }
        const int didx = poker::deck_index_from_card(c);
        if (seen[didx]) {
            return true;
        }
        seen[didx] = true;
    }
    return false;
}

bool try_parse_cards_from_js(Napi::Env env, const Napi::Value& v, std::vector<poker::Card>& out, std::string* err) {
    std::string local_err;
    out = parse_cards_from_js(env, v, err ? err : &local_err);
    const std::string& emsg = err ? *err : local_err;
    if (!emsg.empty()) {
        return fail_type(env, emsg);
    }
    return true;
}

bool try_parse_hole_and_board(Napi::Env env, const Napi::CallbackInfo& info, std::vector<poker::Card>& hole,
                              std::vector<poker::Card>& board, const char* signature) {
    if (info.Length() < 2) {
        return fail_type(env, signature);
    }
    if (!try_parse_cards_from_js(env, info[0], hole)) {
        return false;
    }
    if (!try_parse_cards_from_js(env, info[1], board)) {
        return false;
    }
    return true;
}

Napi::Object eval_to_object(Napi::Env env, const poker::HandEvaluation& e, EvalObjectFormat format) {
    Napi::Object o = Napi::Object::New(env);
    const poker::HandRank cat = poker::hand_category(e);
    const int rank_category = static_cast<int>(cat);
    const double strength = static_cast<double>(poker::pack_hand_strength(e));
    o.Set("rankCategory", Napi::Number::New(env, rank_category));
    o.Set("strength", Napi::Number::New(env, strength));
    if (format == EvalObjectFormat::Slim) {
        return o;
    }
    o.Set("rank", hand_rank_string_interned(env, cat));
    Napi::Array kickers = Napi::Array::New(env, 5);
    for (size_t i = 0; i < e.kickers.size(); ++i) {
        kickers[i] = Napi::Number::New(env, e.kickers[i]);
    }
    o.Set("kickers", kickers);
    return o;
}

bool strings_from_js_array(const Napi::Array& a, const char* ctx, std::vector<std::string>& out, std::string* err) {
    out.clear();
    const uint32_t n = a.Length();
    out.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Napi::Value x = a[i];
        if (!x.IsString()) {
            if (err) {
                *err = std::string(ctx) + ": array must contain only strings";
            }
            return false;
        }
        out.push_back(x.As<Napi::String>().Utf8Value());
    }
    return true;
}

}  // namespace poker_bind
