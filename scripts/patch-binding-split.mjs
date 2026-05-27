import fs from 'node:fs';
import path from 'node:path';

const file = path.join('native', 'binding.cpp');
let s = fs.readFileSync(file, 'utf8');

const headerEnd = s.indexOf('namespace {');
const evaluateStart = s.indexOf('Napi::Value EvaluateBestHand');
if (headerEnd < 0 || evaluateStart < 0) {
  throw new Error('markers not found');
}

const newHead = `#include <napi.h>

#include "async_workers.hpp"
#include "binding_batch.hpp"
#include "binding_cards.hpp"
#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "binding_state.hpp"
#include "poker/card_string.hpp"
#include "poker/game_state.hpp"
#include "poker/hand_evaluator.hpp"
#include "poker/monte_carlo.hpp"
#include "poker/opponent_model.hpp"
#include "poker/poker_math.hpp"
#include "poker/icm.hpp"
#include "poker/side_pot.hpp"
#include "poker/exact_equity.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/strategy.hpp"
#include "poker/types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using poker_bind::DecideActionParsed;
using poker_bind::action_name;
using poker_bind::doubles_from_js_array;
using poker_bind::eval_to_object;
using poker_bind::hand_rank_js;
using poker_bind::is_card_input;
using poker_bind::matrix_from_js_array;
using poker_bind::packed_card_bytes;
using poker_bind::parse_cards_from_js;
using poker_bind::parse_decide_action_inputs;
using poker_bind::parse_return_format;
using poker_bind::read_f64_vector;
using poker_bind::strings_from_js_array;
using poker_bind::write_f64_matrix_flat;
using poker_bind::write_f64_vector;
using poker_bind::F64ReturnFormat;

`;

let body = s.slice(evaluateStart);

// Remove sim handlers (now in binding_batch.cpp)
body = body.replace(
  /Napi::Value SimulateHandOutcome\(const Napi::CallbackInfo& info\) \{[\s\S]*?^}\r?\n\r?\nNapi::Value DecideAction/m,
  'Napi::Value DecideAction',
);

// Remove duplicate strings_from_js_array
body = body.replace(
  /\[\[nodiscard\]\] std::vector<std::string> strings_from_js_array\(const Napi::Array& a, const char\* ctx\) \{[\s\S]*?^}\r?\n\r?\nNapi::Value ValidateCardString/m,
  'Napi::Value ValidateCardString',
);

// Update DecideAction to use poker_bind types (already using parse_decide_action_inputs)
body = body.replace(/DecideActionParsed args\{\};/g, 'poker_bind::DecideActionParsed args{};');

fs.writeFileSync(file, newHead + body);
console.log('patched', file);
