#pragma once

#include "poker/card.hpp"

#include <string>
#include <vector>

namespace poker {

/// Trim ASCII whitespace; parses ranks `23456789TJQKA` and `10`, suits `cdhs` (case-insensitive rank/suit).
[[nodiscard]] bool parse_card_string(const std::string& raw, Card& out);

/// True if any two entries parse to the same card. Throws if any string is not a valid card.
[[nodiscard]] bool card_strings_have_duplicate(const std::vector<std::string>& cards);

}  // namespace poker
