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

enum class EvalObjectFormat { Full, Slim };

[[nodiscard]] std::vector<poker::Card> parse_card_strings(const Napi::Env& env, const Napi::Array& arr,
                                                            std::string* err);

[[nodiscard]] bool parse_card_string_from_js(const Napi::Env& env, const Napi::String& str, poker::Card& out,
                                              std::string* err);

[[nodiscard]] bool is_card_input(const Napi::Value& v);

[[nodiscard]] bool packed_card_bytes(const Napi::Value& v, const std::uint8_t** data, std::size_t* len,
                                     std::string* err);

[[nodiscard]] std::vector<poker::Card> parse_cards_from_js(const Napi::Env& env, const Napi::Value& v,
                                                             std::string* err);

[[nodiscard]] bool js_card_array_has_duplicate(const Napi::Env& env, const Napi::Array& arr, std::string* err);

/** Sets TypeError and returns false on parse failure; out filled on success. */
[[nodiscard]] bool try_parse_cards_from_js(Napi::Env env, const Napi::Value& v, std::vector<poker::Card>& out,
                                           std::string* err = nullptr);

[[nodiscard]] bool try_parse_hole_and_board(Napi::Env env, const Napi::CallbackInfo& info,
                                            std::vector<poker::Card>& hole, std::vector<poker::Card>& board,
                                            const char* signature);

[[nodiscard]] Napi::Object eval_to_object(Napi::Env env, const poker::HandEvaluation& e,
                                          EvalObjectFormat format = EvalObjectFormat::Full);

[[nodiscard]] bool strings_from_js_array(const Napi::Array& a, const char* ctx, std::vector<std::string>& out,
                                         std::string* err);

}  // namespace poker_bind
