#pragma once

#include <napi.h>

#include "poker/range.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace poker_bind {

/**
 * Deck ids of a `SparseRangeSpec.indices` value: `number[]`, `Int32Array`, or `Uint32Array`. Every
 * entry must be an integer in 0..51, checked on the original value before any conversion, so NaN,
 * 1.9, -1, or 2^32 + 1 are rejected instead of becoming a different card.
 */
[[nodiscard]] bool read_range_deck_ids(const Napi::Value& v, std::vector<int>& out, std::string* err);

/**
 * `Float64Array(1326)` or `{ indices, weights }` as a sparse range without combos that touch
 * `dead_mask`. Weights must be finite and non-negative; a combo listed more than once adds up.
 * Returns false with `err` set on any invalid input (including a range left empty by blockers).
 */
[[nodiscard]] bool parse_sparse_range_spec(const Napi::Value& v, std::uint64_t dead_mask, poker::SparseRange& out,
                                           std::string* err);

/** The same inputs as dense weights over the 1326 combos (no blockers applied). */
[[nodiscard]] bool read_dense_range_spec(const Napi::Value& v, std::vector<double>& out, std::string* err);

}  // namespace poker_bind
