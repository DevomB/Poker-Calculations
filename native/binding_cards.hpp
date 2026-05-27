#pragma once

#include <napi.h>

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace poker_bind {

[[nodiscard]] std::vector<poker::Card> parse_card_strings(const Napi::Env& env, const Napi::Array& arr,
                                                            std::string* err);

[[nodiscard]] bool is_card_input(const Napi::Value& v);

[[nodiscard]] bool packed_card_bytes(const Napi::Value& v, const std::uint8_t** data, std::size_t* len,
                                     std::string* err);

[[nodiscard]] std::vector<poker::Card> parse_cards_from_js(const Napi::Env& env, const Napi::Value& v,
                                                             std::string* err);

[[nodiscard]] std::string hand_rank_js(poker::HandRank r);

[[nodiscard]] Napi::Object eval_to_object(Napi::Env env, const poker::HandEvaluation& e);

[[nodiscard]] std::vector<std::string> strings_from_js_array(const Napi::Array& a, const char* ctx);

}  // namespace poker_bind
