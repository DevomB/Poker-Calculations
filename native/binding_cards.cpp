#include "binding_cards.hpp"

#include "poker/card_string.hpp"

#include <stdexcept>

namespace poker_bind {

namespace {

constexpr const char* kHandRankNames[] = {"highCard",      "onePair",       "twoPair",    "threeOfAKind",
                                          "straight",      "flush",         "fullHouse",  "fourOfAKind",
                                          "straightFlush", "royalFlush"};

}  // namespace

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
        if (!poker::parse_card_string(v.As<Napi::String>().Utf8Value(), c)) {
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

std::string hand_rank_js(poker::HandRank r) {
    const int idx = static_cast<int>(r);
    if (idx >= 0 && idx < 10) {
        return kHandRankNames[idx];
    }
    return "unknown";
}

Napi::Object eval_to_object(Napi::Env env, const poker::HandEvaluation& e) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("rank", hand_rank_js(poker::hand_category(e)));
    Napi::Array kickers = Napi::Array::New(env, 5);
    for (size_t i = 0; i < e.kickers.size(); ++i) {
        kickers[i] = Napi::Number::New(env, e.kickers[i]);
    }
    o.Set("kickers", kickers);
    return o;
}

std::vector<std::string> strings_from_js_array(const Napi::Array& a, const char* ctx) {
    std::vector<std::string> v;
    const uint32_t n = a.Length();
    v.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Napi::Value x = a[i];
        if (!x.IsString()) {
            throw std::invalid_argument(std::string(ctx) + ": array must contain only strings");
        }
        v.push_back(x.As<Napi::String>().Utf8Value());
    }
    return v;
}

}  // namespace poker_bind
